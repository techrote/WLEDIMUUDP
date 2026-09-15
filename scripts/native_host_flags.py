Import("env")

import os

if os.name == "nt":
    env.Append(LIBS=["ws2_32"])
