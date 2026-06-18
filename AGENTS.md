# Guia para agentes de codigo

Este repo implementa `pocketsynth`, un sintetizador real-time para M5Stack
Cardputer ADV. Antes de cambiar codigo, lee `docs/iteration-1.md`,
`docs/architecture.md` y `docs/ui.md`.

## Prioridades del proyecto

1. Audio estable.
2. Interaccion de teclado fiable.
3. UI compacta que refleje estado real.
4. Deteccion musical basica.
5. Funciones avanzadas solo despues de cerrar la iteracion 1.

No introduzcas ADSR, LFO, filtros, presets, secuenciador, arpegiador ni menus
profundos durante la iteracion 1 salvo que el usuario lo pida explicitamente.

## Reglas del camino de audio

La tarea o funcion que renderiza audio no debe hacer:

- Logs (`printf`, `ESP_LOG*`).
- `malloc`, `new`, `std::vector` dinamico o `String`.
- Lectura de SD.
- Redibujado de pantalla.
- I2C lento.
- Esperas de mutex largas.
- Trabajo de UI o analisis musical pesado.

El audio debe poder renderizar buffers de forma predecible y escribirlos a I2S.

## Arquitectura esperada

- `AudioTask`: prioridad alta, genera buffers y escribe I2S.
- `InputTask`: prioridad media, escanea teclado y emite eventos.
- `SynthControlTask`: prioridad media, aplica note on/off, waveform y volumen.
- `UiTask`: prioridad baja, redibuja solo cuando hay cambios.
- `ChordTask`: opcional; puede estar integrado en control al principio.

Si hay que compartir estado con audio, usa copias pequenas o secciones criticas
cortas. No bloquees el render de audio esperando a la UI.

## Layout del repo

- `src/` e `include/`: firmware principal del sintetizador.
- `lib/firmware/cardputer_adv_ui_test`: banco de pruebas de display, teclado,
  widgets y runtime de UI generado.
- `docs/references/cardputer-ui-pocketsynth-main.cardputer-ui.json`: fuente UI
  de referencia mas reciente.
- `docs/references/pocket_synth_iteration_1_design_v1_2.pdf`: documento
  original de diseno.

## Validacion

Para cambios de firmware principal:

```powershell
pio run -e cardputer_adv
```

Para cambios en el banco de pruebas de UI:

```powershell
pio run -d lib/firmware/cardputer_adv_ui_test -e cardputer_adv
```

Documenta en la respuesta final si no se pudo compilar o probar en hardware.
