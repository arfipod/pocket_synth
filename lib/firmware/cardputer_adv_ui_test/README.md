# cardputer_adv_ui_test

Banco de pruebas de UI para M5Stack Cardputer ADV.

Este proyecto sirve para validar:

- Inicializacion de display ST7789V2.
- Framebuffer RGB565.
- Widgets basicos.
- Teclado fisico y modificadores.
- Runtime de UI generado.
- Dumps de framebuffer por serial.

## Build

```powershell
pio run -d lib/firmware/cardputer_adv_ui_test -e cardputer_adv
```

## Fuente UI

El proyecto incluye un JSON de UI en:

```text
generated-project.cardputer-ui.json
```

La referencia mas reciente compartida para `pocketsynth` esta archivada en:

```text
docs/references/cardputer-ui-pocketsynth-main.cardputer-ui.json
```

El JSON local de este proyecto puede tener pequenas diferencias de coordenadas
respecto a esa referencia archivada. Tratar `docs/references/` como fuente de
diseno mas reciente y este proyecto como runtime/banco de pruebas.

Si se regenera UI, mantener sincronizados el JSON fuente y los archivos de
`src/generated/`.

## Comandos seriales

| Comando | Accion |
| --- | --- |
| `fb` o `dump` | Dump de framebuffer en orden nativo del panel. |
| `fb logical` | Dump de framebuffer en orden logico. |
| `widgets` o `gallery` | Muestra la galeria de widgets. |
| `ui` o `generated` | Vuelve a la UI generada. |

## Integracion futura

Para el firmware principal de `pocketsynth`, este proyecto debe tratarse como
base de runtime y laboratorio de UI. La integracion final debe conectar los
widgets al estado real del sintetizador: notas activas, waveform, volumen,
polifonia y acorde detectado.
