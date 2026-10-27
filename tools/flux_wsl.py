"""Paints pictures locally with FLUX.2-dev in WSL2 (Ubuntu-22.04, the conda env "flux" of Text2Video, its model cache):
the prompts are encoded first by Mistral-3 in a process of their own (then the GPU memory is free), the transformer runs
in 4 bits. Windows paths (G:/...) in the tasks are read as /mnt/g/... .

    wsl -d Ubuntu-22.04 -- ~/miniconda3/envs/flux/bin/python /mnt/g/Tools/Games/SpaceOpera/tools/flux_wsl.py TASKS.json

TASKS.json: [{"prompt": ..., "out": "path.jpg", "width": 1024, "height": 1024, "seed": 1}, ...]; existing outputs are
skipped. make_textures.py, make_dress.py and make_items.py hand their jobs here with --local.
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
    quant = BitsAndBytesConfig( load_in_4bit=True, bnb_4bit_compute_dtype=torch.bfloat16, bnb_4bit_quant_type="nf4",
                                bnb_4bit_use_double_quant=True )
    text = Mistral3ForConditionalGeneration.from_pretrained( MODEL, subfolder="text_encoder", quantization_config=quant,
                                                             dtype=torch.bfloat16, device_map="auto", cache_dir=CACHE )
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

    embeds = torch.load( embeds_path, weights_only=True )
    quant = BitsAndBytesConfig( load_in_4bit=True, bnb_4bit_compute_dtype=torch.bfloat16, bnb_4bit_quant_type="nf4",
                                bnb_4bit_use_double_quant=True )
    transformer = Flux2Transformer2DModel.from_pretrained( MODEL, subfolder="transformer", quantization_config=quant,
                                                           torch_dtype=torch.bfloat16, device_map={ "": 0 },
                                                           cache_dir=CACHE )
    vae = AutoencoderKLFlux2.from_pretrained( MODEL, subfolder="vae", torch_dtype=torch.bfloat16,
                                              cache_dir=CACHE ).to( "cuda" )
    pipe = Flux2Pipeline.from_pretrained( MODEL, transformer=transformer, vae=vae, text_encoder=None, tokenizer=None,
                                          torch_dtype=torch.bfloat16, cache_dir=CACHE )
    from PIL import Image  # noqa: F401  (saving)
    for t, e in zip( tasks, embeds ):
        try:
            g = torch.Generator( "cuda" ).manual_seed( int( t.get( "seed", 1 ) ) )
            image = pipe( prompt_embeds=e.to( "cuda", dtype=torch.bfloat16 ), guidance_scale=t.get( "guidance", 4.0 ),
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
