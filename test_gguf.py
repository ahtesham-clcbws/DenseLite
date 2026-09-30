import sys
import struct

def read_gguf(file_path):
    with open(file_path, "rb") as f:
        magic = f.read(4)
        if magic != b"GGUF":
            print("Not a GGUF file")
            return
        
        version = struct.unpack("<I", f.read(4))[0]
        tensor_count = struct.unpack("<Q", f.read(8))[0]
        kv_count = struct.unpack("<Q", f.read(8))[0]
        
        print(f"Version: {version}, Tensors: {tensor_count}")
        # Not reading all the KVs because it's complex, we just want to know if it's Q4_K.

read_gguf("/home/ahtesham/.denselite/models/Llama-3.2-1B-Instruct-abliterated.i1-Q4_K_M.gguf")
