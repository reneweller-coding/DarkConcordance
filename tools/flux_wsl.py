"""Paints pictures locally with FLUX.2-dev in WSL2 (Ubuntu-22.04, the conda env "flux" of Text2Video, its model cache):
the prompts are encoded first by Mistral-3 in a process of their own (then the GPU memory is free), the transformer runs
in 4 bits. Windows paths (G:/...) in the tasks are read as /mnt/g/... .

    wsl -d Ubuntu-22.04 -- ~/miniconda3/envs/flux/bin/python /mnt/g/Tools/Games/DarkConcordance/tools/flux_wsl.py TASKS.json

TASKS.json: [{"prompt": ..., "out": "path.jpg", "width": 1024, "height": 1024, "seed": 1, "refs": [...]}, ...];
existing outputs are skipped; "refs" are pictures FLUX.2 takes faces and things from (the films' portraits).
make_textures.py, make_dress.py and make_items.py hand their jobs here with --local.

The first run reads the bf16 weights (about 100 GB on G:, through WSL's slow 9P bridge) and turns them into 4 bits --
some 15 minutes -- and keeps the 4-bit transformer and text encoder on WSL's own disk (LOCAL, about 30 GB); every
later run loads those in under a minute.
"""
import argparse
import gc
import json
import os
import re
import subprocess
import sys

CACHE = "/mnt/g/Tools/Text2Video/models/huggingface_cache"
MODEL = "black-forest-labs/FLUX.2-dev"
LOCAL = os.path.expanduser( "~/models/flux2-dev-nf4" )


def keep( model, name ):
    """The model just quantised, saved to LOCAL/name (written beside it first, so a broken save leaves nothing)."""
    dst = os.path.join( LOCAL, name )
    tmp = dst + ".part"
    try:
        import shutil
        import time
        t0 = time.time()
        shutil.rmtree( tmp, ignore_errors=True )
        model.save_pretrained( tmp, max_shard_size="200GB" )   # one file: a 4-bit weight and its stats together
        os.replace( tmp, dst )
        print( "kept the 4-bit %s in %s (%.0f s)" % ( name, dst, time.time() - t0 ), flush=True )
    except Exception as ex:  # noqa: BLE001  (painting goes on without the copy)
        print( "could not keep the 4-bit %s: %s" % ( name, ex ), flush=True )


def kept( name ):
    path = os.path.join( LOCAL, name )
    return path if os.path.exists( os.path.join( path, "config.json" ) ) else None


def linux( path ):
    """G:/a/b or G:\\a\\b -> /mnt/g/a/b."""
    path = path.replace( "\\", "/" )
    m = re.match( r"^([A-Za-z]):/(.*)$", path )
    return "/mnt/%s/%s" % ( m.group( 1 ).lower(), m.group( 2 ) ) if m else path


def encode( tasks, out ):
    """Phase 1 (its own process): every prompt through Mistral-3 in 4 bits, the embeddings to disk."""
    import torch
    from diffusers import Flux2Pipeline
    from transformers import BitsAndBytesConfig, Mistral3ForConditionalGeneration

    # FLUX.2's own text encoder (Mistral Small 3.2) and processor from its repository, the encoder in 4 bits.
    import time
    t0 = time.time()
    if kept( "text_encoder" ):
        text = Mistral3ForConditionalGeneration.from_pretrained( kept( "text_encoder" ), dtype=torch.bfloat16,
                                                                 device_map="auto" )
    else:
        quant = BitsAndBytesConfig( load_in_4bit=True, bnb_4bit_compute_dtype=torch.bfloat16,
                                    bnb_4bit_quant_type="nf4", bnb_4bit_use_double_quant=True )
        text = Mistral3ForConditionalGeneration.from_pretrained( MODEL, subfolder="text_encoder",
                                                                 quantization_config=quant, dtype=torch.bfloat16,
                                                                 device_map="auto", cache_dir=CACHE )
        keep( text, "text_encoder" )
    print( "text encoder loaded (%.0f s)" % ( time.time() - t0 ), flush=True )
    pipe = Flux2Pipeline.from_pretrained( MODEL, transformer=None, vae=None, text_encoder=text, dtype=torch.bfloat16,
                                          cache_dir=CACHE )
    embeds = []
    with torch.inference_mode():
        for t in tasks:
            e, _ = pipe.encode_prompt( prompt=t["prompt"], max_sequence_length=512 )
            embeds.append( e.cpu() )
            print( "encoded", os.path.basename( t["out"] ), flush=True )
    torch.save( embeds, out )


def paint( tasks, embeds_path ):
    """Phase 2: the transformer in 4 bits, the VAE in bf16, one picture per task."""
    import torch
    from diffusers import AutoencoderKLFlux2, Flux2Pipeline, Flux2Transformer2DModel
    from diffusers import BitsAndBytesConfig

    import time
    embeds = torch.load( embeds_path, weights_only=True )
    t0 = time.time()
    if kept( "transformer" ):
        transformer = Flux2Transformer2DModel.from_pretrained( kept( "transformer" ), torch_dtype=torch.bfloat16,
                                                               device_map={ "": 0 } )
    else:
        quant = BitsAndBytesConfig( load_in_4bit=True, bnb_4bit_compute_dtype=torch.bfloat16,
                                    bnb_4bit_quant_type="nf4", bnb_4bit_use_double_quant=True )
        transformer = Flux2Transformer2DModel.from_pretrained( MODEL, subfolder="transformer",
                                                               quantization_config=quant, torch_dtype=torch.bfloat16,
                                                               device_map={ "": 0 }, cache_dir=CACHE )
        keep( transformer, "transformer" )
    print( "transformer loaded (%.0f s)" % ( time.time() - t0 ), flush=True )
    vae = AutoencoderKLFlux2.from_pretrained( MODEL, subfolder="vae", torch_dtype=torch.bfloat16,
                                              cache_dir=CACHE ).to( "cuda" )
    pipe = Flux2Pipeline.from_pretrained( MODEL, transformer=transformer, vae=vae, text_encoder=None, tokenizer=None,
                                          torch_dtype=torch.bfloat16, cache_dir=CACHE )
    from PIL import Image
    for t, e in zip( tasks, embeds ):
        try:
            g = torch.Generator( "cuda" ).manual_seed( int( t.get( "seed", 1 ) ) )
            refs = [Image.open( linux( r ) ).convert( "RGB" ) for r in t.get( "refs", [] )] or None
            image = pipe( prompt_embeds=e.to( "cuda", dtype=torch.bfloat16 ), image=refs,
                          guidance_scale=t.get( "guidance", 4.0 ),
                          num_inference_steps=t.get( "steps", 28 ), height=t.get( "height", 1024 ),
                          width=t.get( "width", 1024 ), generator=g ).images[0]
            out = linux( t["out"] )
            os.makedirs( os.path.dirname( out ), exist_ok=True )
            image.convert( "RGB" ).save( out, quality=95 )
            print( "painted", t["out"], flush=True )
        except Exception as ex:  # noqa: BLE001  (one picture failing must not stop the rest)
            print( "failed", t["out"], ex, flush=True )
    del pipe
    gc.collect()
    torch.cuda.empty_cache()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument( "tasks" )
    ap.add_argument( "--encode-only", default=None, help="(internal) phase 1 into this file" )
    args = ap.parse_args()
    spec = linux( args.tasks )
    tasks = [t for t in json.load( open( spec, encoding="utf-8" ) ) if not os.path.exists( linux( t["out"] ) )]
    if not tasks:
        print( "nothing to paint" )
        return 0
    embeds = os.path.splitext( spec )[0] + ".embeds.pt"
    if args.encode_only:
        encode( tasks, args.encode_only )
        return 0
    subprocess.run( [sys.executable, os.path.abspath( __file__ ), spec, "--encode-only", embeds], check=True )
    paint( tasks, embeds )
    return 0


if __name__ == "__main__":
    sys.exit( main() )
