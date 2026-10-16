# Painel N16R8

Painel para o kit ESP32-S3 N16R8-IPEX-340 com a base e a tela de 2,8 polegadas. Cicla clima, câmbio, gráfico, cripto e ações. A configuração fica numa página no navegador.

## Ligação

O cabo de dados entra no USB-TTL da ESP32. A USB-C da base fica sem cabo.

## Gravar

```text
python -m pip install platformio==6.1.19
python -m platformio run -e painel -t upload --upload-port COM5
```

Troque `COM5` pela porta USB-SERIAL CH340.

## Configurar

Na primeira vez a tela pede a rede `Painel-N16R8`. Abra `http://192.168.4.1`, escolha o Wi-Fi de casa e a senha. Depois a tela mostra o endereço na rede de casa.
