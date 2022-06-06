import math
import codecs

CHUNK_SIZE = 7
KEY_SIZE = 4

# Step 0: read the data
with open(r"./secret/file.txt.lock", "rb") as fh:
    data = fh.read()

# Step 1: read the key using the xored null bytes of UTF16-LE encoding while skipping BOM
key = [0xff] * KEY_SIZE
found = 0
for i in range(
    KEY_SIZE - 1, # Skip BOM
    len(data)
):
    if found == KEY_SIZE:
        break

    if i % 2: # Even bytes reveal one byte of the key
        chunk_idx = (int)(i / CHUNK_SIZE) + 1
        if found == 0:
            key[0] = data[i]  ^ chunk_idx
        elif found == 1:
            key[2] = data[i]  ^ chunk_idx
        elif found == 2:
            key[3] = data[i]  ^ chunk_idx
        elif found == 3:
            key[1] = data[i]  ^ chunk_idx

        found += 1

# Little endian repr
key = key[::-1]

print("Key: ", end="")
for k in key:
    print(f"{k:#x}, ", end="")
print("")

# Step 2: decode data
def chunks(xs, n):
    for i in range(0, len(xs), n):
        yield xs[i:i + n]

print("Chunks:")
clear = []
chunks = list(chunks(data, CHUNK_SIZE))
for chunk_idx, chunk in enumerate(chunks):
    for chunk_offset, byte in enumerate(chunk):
        # We can use chunk_offset because CHUNK_SIZE > KEY_SIZE, we don't need an offset from the beginning here
        byte = byte ^ key[chunk_offset % KEY_SIZE] ^ (chunk_idx + 1)
        print(f"0x{byte:02x}, ", end="")
        clear.append(byte)
        #print(f"{byte:c}", end="")
    print("")

# Step 3: reconstruct the text file
with open("clear.txt", "wb") as fh:
    decoded = bytes(clear).decode("utf-16-le")
    fh.write(decoded.encode('utf-16-le'))
    print(f"Contents: {decoded}")
