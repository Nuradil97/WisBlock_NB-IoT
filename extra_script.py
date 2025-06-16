import os
import struct
from SCons.Script import Import

Import("env")

# Constants for UF2 creation
UF2_MAGIC_START0 = 0x0A324655  # "UF2\n"
UF2_MAGIC_START1 = 0x9E5D5157  # Randomly selected
UF2_MAGIC_END = 0x0AB16F30  # Ditto
FAMILY_ID = 0xADA52840


# Custom HEX from ELF
def generate_hex_action(source, target, env):
    env.Execute(env.VerboseAction(" ".join([
        "$OBJCOPY", "-O", "ihex", "-R", ".eeprom",
        "$BUILD_DIR/${PROGNAME}.elf", "$BUILD_DIR/${PROGNAME}.hex"
    ]), "Building $BUILD_DIR/${PROGNAME}.hex"))


# Create UF2 from HEX
def create_uf2_action(source, target, env):
    source_hex = target[0].get_abspath()
    target_uf2 = source_hex.replace(".hex", ".uf2")

    print("#########################################################")
    print(f"Create UF2 from {source_hex}")
    print("#########################################################")

    with open(source_hex, mode="r") as f:
        hex_data = f.read()

    uf2_data = convert_from_hex_to_uf2(hex_data)
    write_file(target_uf2, uf2_data)

    print(f"#########################################################")
    print(f"{target_uf2} is ready to flash to target device")
    print("#########################################################")


# Write binary data to a file
def write_file(name, buf):
    with open(name, "wb") as f:
        f.write(buf)


# Convert Intel HEX to UF2
def convert_from_hex_to_uf2(hex_data):
    blocks = []
    upper = 0
    curr_block = None

    for line in hex_data.splitlines():
        if not line.startswith(":"):
            continue

        record = bytearray.fromhex(line[1:])
        record_type = record[3]

        if record_type == 0x04:
            upper = ((record[4] << 8) | record[5]) << 16
        elif record_type == 0x00:
            address = upper | (record[1] << 8) | record[2]
            data = record[4:-1]

            for i, byte in enumerate(data):
                if not curr_block or curr_block["address"] & ~0xFF != (address + i) & ~0xFF:
                    curr_block = {"address": (address + i) & ~0xFF, "data": bytearray(256)}
                    blocks.append(curr_block)

                curr_block["data"][(address + i) & 0xFF] = byte

    uf2_blocks = []
    num_blocks = len(blocks)
    for i, block in enumerate(blocks):
        uf2_blocks.append(encode_uf2_block(block, i, num_blocks))

    return b"".join(uf2_blocks)


# Encode a block for UF2
def encode_uf2_block(block, block_num, num_blocks):
    header = struct.pack(
        "<IIIIIIII",
        UF2_MAGIC_START0,
        UF2_MAGIC_START1,
        0x2000 if FAMILY_ID else 0x0,
        block["address"],
        256,
        block_num,
        num_blocks,
        FAMILY_ID,
    )
    data = block["data"]
    padding = bytearray(512 - len(header) - len(data) - 4)
    footer = struct.pack("<I", UF2_MAGIC_END)

    return header + data + padding + footer


# Add post actions to generate HEX and UF2 files
env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", generate_hex_action)
env.AddPostAction("$BUILD_DIR/${PROGNAME}.hex", create_uf2_action)
