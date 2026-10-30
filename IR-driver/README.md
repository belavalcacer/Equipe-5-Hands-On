# DevTitans IR USB Driver

## Português

### Compilar e carregar o driver

Compile o módulo para o kernel atualmente em execução e carregue-o:

```bash
make
sudo ./driver.sh load
sudo ./driver.sh status
```

O comando `load` conecta o FT232 ao driver customizado e cria `/dev/devtitans_ir0`. Para remover o driver e devolver o dispositivo ao `ftdi_sio` (normalmente recriando `/dev/ttyUSB*`), use:

```bash
sudo ./driver.sh unload
```

Outros comandos disponíveis: `sudo ./driver.sh restart` e `sudo ./driver.sh status`.

### Usar o CLI

O `ir_cli.py` comunica pelo char device do driver e não usa `pyserial`:

```bash
sudo python3 ir_cli.py ping
sudo python3 ir_cli.py example /tmp/ir-payload.bin
sudo python3 ir_cli.py write /tmp/ir-payload.bin
sudo python3 ir_cli.py read /tmp/captura.bin
```

`ping` verifica a resposta do dispositivo. `example` cria um payload RAW pequeno. `write` envia um arquivo que contém somente o payload IR. `read` captura uma resposta READ e salva somente o payload IR; para depois reproduzir a captura, passe esse mesmo arquivo a `write`:

```bash
sudo python3 ir_cli.py read /tmp/captura.bin
sudo python3 ir_cli.py write /tmp/captura.bin
```

Sem nome de arquivo, `read` salva em `./ir_capture.bin`. Para alterar o timeout ou o device node, use opções antes do subcomando, por exemplo:

```bash
sudo python3 ir_cli.py --timeout 30 --device /dev/devtitans_ir0 read /tmp/captura.bin
```

## English

### Build and load the driver

Build the module for the currently running kernel, then load it:

```bash
make
sudo ./driver.sh load
sudo ./driver.sh status
```

`load` attaches the FT232 to the custom driver and creates `/dev/devtitans_ir0`. To unload the driver and return the device to `ftdi_sio` (normally restoring `/dev/ttyUSB*`), run:

```bash
sudo ./driver.sh unload
```

Other available commands are `sudo ./driver.sh restart` and `sudo ./driver.sh status`.

### Use the CLI

`ir_cli.py` communicates through the driver's character device and does not use `pyserial`:

```bash
sudo python3 ir_cli.py ping
sudo python3 ir_cli.py example /tmp/ir-payload.bin
sudo python3 ir_cli.py write /tmp/ir-payload.bin
sudo python3 ir_cli.py read /tmp/capture.bin
```

`ping` checks the device response. `example` creates a small RAW payload. `write` sends a file containing only the IR payload. `read` requests a frame and saves only its IR payload; replay it by passing the same file to `write`:

```bash
sudo python3 ir_cli.py read /tmp/capture.bin
sudo python3 ir_cli.py write /tmp/capture.bin
```

If no output filename is given, `read` saves to `./ir_capture.bin`. Set the timeout or device node with options before the subcommand, for example:

```bash
sudo python3 ir_cli.py --timeout 30 --device /dev/devtitans_ir0 read /tmp/capture.bin
```
