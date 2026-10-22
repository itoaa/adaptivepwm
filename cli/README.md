# Host CLI tools (optional)

`pwm_cli.py` är ett ofärdigt värdprogram. Det öppnar **inte** serieporten och pratar inte med firmware-CLI:t. Det simulerar parametrar och platshållarcertifikat (`init`, `status`, `set`, `safety`).

Firmware-sanningen är kommandona i `App/Src/cli_commands.c`. Första korttestet använder ST-Link-serieporten, 115200 8N1: `help`, `status`, `config vout`, `pwm`, `monitor`.

`pwmctl.c` är ett separat C-bygge mot OpenSSL och ingår inte i `make` i roten. Prioritet är firmware + HIL, inte denna mapp.
