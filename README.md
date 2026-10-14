# Painel N16R8

Demonstração para o kit ESP32-S3 N16R8-IPEX-340 com a base e a tela de 2,8 polegadas (ILI9341).

A tela mostra o nome, um radar e as redes Wi-Fi mais fortes. O LED RGB da placa acompanha o radar.

## Ligação

O cabo de dados entra no USB-TTL da ESP32. A USB-C da base fica sem cabo.

A ESP32 encaixa nos soquetes de 21 pinos da base, com o GND do lado da antena no GND da base. A tela entra com VCC no VCC.

## Gravar

Requer Python e PlatformIO Core 6.1.19.

```text
python -m pip install platformio==6.1.19
python -m platformio run -e painel -t upload --upload-port COM5
python -m platformio device monitor --port COM5 --baud 115200
```

Troque `COM5` pela porta que o Windows mostrar (USB-SERIAL CH340).
