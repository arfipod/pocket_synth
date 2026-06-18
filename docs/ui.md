# UI y controles

## Fuente de referencia

La UI compartida esta guardada en:

```text
docs/references/cardputer-ui-pocketsynth-main.cardputer-ui.json
```

El banco de pruebas/runtime de UI existente esta en:

```text
lib/firmware/cardputer_adv_ui_test
```

Ese proyecto contiene display, teclado, widgets, input mapper, UI generada y
comandos de debug por serial. Es la base practica para integrar o validar la
interfaz de `pocketsynth`.

Nota de sincronizacion: el JSON archivado en `docs/references/` es la version
mas reciente compartida. El JSON dentro de `lib/firmware/cardputer_adv_ui_test`
puede tener pequenas diferencias de coordenadas porque pertenece al banco de
pruebas generado anteriormente.

## Dispositivo

- M5Stack Cardputer ADV.
- Orientacion landscape.
- Resolucion logica: 240 x 135 px.
- Color: RGB565 / ST7789V2.

## Pantalla principal

Una sola pantalla: `screen-main`.

Resumen de elementos del JSON:

| Tipo | Cantidad | Uso |
| --- | ---: | --- |
| `text` | 35 | Titulos, labels, notas, acorde y marcas de octava. |
| `progress` | 2 | Bateria y volumen maestro. |
| `button` | 29 | Selectores de onda y teclas de piano. |
| `sparkline` | 6 | Iconos de onda y previews. |

Elementos principales:

- `text-title`: `pocketsynth`.
- `text-polyphony`: `0/8`.
- `text-chord-label`: `CHORD`.
- `text-current-chord`: `--`.
- `progress-battery`: bateria.
- `progress-main-volume`: volumen vertical.
- `sparkline-wave-preview`: preview de onda seleccionada.
- `sparkline-output-preview`: preview de salida polifonica.

El documento de diseno original menciona `VOICE: V1` como posible texto de
estado. El JSON mas reciente no contiene ese elemento; para la iteracion 1 se
puede omitir mientras la pantalla conserve claro que solo hay una voz.

## Selectores de forma de onda

| Elemento | Tecla | Forma | Icono |
| --- | --- | --- | --- |
| `button-wave-sine` | `Fn+1` | Senoidal | `icon-wave-sine` |
| `button-wave-square` | `Fn+2` | Cuadrada | `icon-wave-square` |
| `button-wave-rectangular` | `Fn+3` | Rectangular | `icon-wave-rectangular` |
| `button-wave-sawtooth` | `Fn+4` | Diente de sierra | `icon-wave-sawtooth` |

Estado activo:

- Fill: `#193322`.
- Stroke: `#9bffb7`.

Estado inactivo:

- Fill: `#101823`.
- Stroke: `#34445d`.

## Piano

Teclas blancas:

| Tecla fisica | Nota | Elemento |
| --- | --- | --- |
| z | C4 | `button-piano-white-z` |
| x | D4 | `button-piano-white-x` |
| c | E4 | `button-piano-white-c` |
| v | F4 | `button-piano-white-v` |
| b | G4 | `button-piano-white-b` |
| n | A4 | `button-piano-white-n` |
| m | B4 | `button-piano-white-m` |
| q | C5 | `button-piano-white-q` |
| w | D5 | `button-piano-white-w` |
| e | E5 | `button-piano-white-e` |
| r | F5 | `button-piano-white-r` |
| t | G5 | `button-piano-white-t` |
| y | A5 | `button-piano-white-y` |
| u | B5 | `button-piano-white-u` |
| i | C6 | `button-piano-white-i` |

Teclas negras:

| Tecla fisica | Nota | Elemento |
| --- | --- | --- |
| s | C#4 / Db4 | `button-piano-black-s` |
| d | D#4 / Eb4 | `button-piano-black-d` |
| g | F#4 / Gb4 | `button-piano-black-g` |
| h | G#4 / Ab4 | `button-piano-black-h` |
| j | A#4 / Bb4 | `button-piano-black-j` |
| 2 | C#5 / Db5 | `button-piano-black-2` |
| 3 | D#5 / Eb5 | `button-piano-black-3` |
| 5 | F#5 / Gb5 | `button-piano-black-5` |
| 6 | G#5 / Ab5 | `button-piano-black-6` |
| 7 | A#5 / Bb5 | `button-piano-black-7` |

Feedback de tecla pulsada:

| Tipo | Fill | Stroke |
| --- | --- | --- |
| Blanca | `#d8ecff` | `#7cc7ff` |
| Negra | `#25415f` | `#7cc7ff` |

## Colores base

| Uso | Color |
| --- | --- |
| Fondo tecnico oscuro | `#0b1018` |
| Texto primario suave | `#c7d7ef` |
| Texto secundario | `#8fa0bb` |
| Estado/acento verde | `#9bffb7` |
| Estado/acento azul | `#7cc7ff` |
| Stroke tenue | `#34445d` |
| Bateria verde | `#76bb40` |

## Banco de pruebas de UI

Comandos de build:

```powershell
pio run -d lib/firmware/cardputer_adv_ui_test -e cardputer_adv
```

Comandos seriales implementados en el runtime:

| Comando | Accion |
| --- | --- |
| `fb` o `dump` | Dump de framebuffer en orden nativo de panel. |
| `fb logical` | Dump de framebuffer en orden logico. |
| `widgets` o `gallery` | Cambia a galeria de widgets. |
| `ui` o `generated` | Vuelve a la UI generada. |

La cabecera del runtime dibuja diagnostico de teclado en la parte superior.
Cuando se integre en `pocketsynth`, esa capa debe convertirse en estado musical
o quedar solo para modo debug.

## Generacion de UI

El proyecto de smoke test documenta este comando, aunque el generador no vive
en este repo:

```powershell
npm run firmware:prepare -- path/to/project.cardputer-ui.json
```

Si se regenera UI, mantener sincronizados:

- JSON fuente.
- `src/generated/cardputer_ui.*`.
- `src/generated/cardputer_ui_assets.*`.
- `src/generated/cardputer_ui_fonts.*`.

## Restricciones de diseno

- Una sola pantalla.
- Texto pequeno y legible.
- Nada de controles para funciones no implementadas.
- Iconos de onda compactos, tecnicos y reconocibles.
- La UI debe reflejar estado real, no solo eventos momentaneos.
- Redibujar a 15-20 FPS maximo y solo cuando cambie algo importante.
