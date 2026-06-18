# pocketsynth

`pocketsynth` es un sintetizador en tiempo real para M5Stack Cardputer ADV.
La primera iteracion busca validar la base critica del instrumento: teclado
fisico, eventos note on/off, osciladores matematicos, mezcla de hasta 8 notas,
volumen maestro, buffer de audio y salida I2S/speaker.

La regla de trabajo para la iteracion 1 es sencilla: primero audio estable,
despues interaccion, despues UI y despues musicalidad.

## Estado actual

- Proyecto principal PlatformIO/ESP-IDF en la raiz del repo.
- Prototipo minimo de osciladores en `include/` y `src/`.
- Banco de pruebas de UI para Cardputer ADV en
  `lib/firmware/cardputer_adv_ui_test`.
- Fuentes de diseno originales guardadas en `docs/references/`.

## Documentacion

- `docs/iteration-1.md`: alcance completo de la primera iteracion.
- `docs/architecture.md`: arquitectura FreeRTOS, estado y reglas real-time.
- `docs/ui.md`: UI compacta, controles, colores y referencia al JSON.
- `docs/implementation-plan.md`: fases 1A-1G con criterios de aceptacion.
- `docs/decisions.md`: decisiones tecnicas iniciales.
- `AGENTS.md`: guia para agentes de codigo que trabajen en este repo.

## Build

Proyecto principal:

```powershell
pio run -e cardputer_adv
```

Banco de pruebas de UI:

```powershell
pio run -d lib/firmware/cardputer_adv_ui_test -e cardputer_adv
```

## Hardware objetivo

- M5Stack Cardputer ADV.
- Pantalla ST7789V2 de 240 x 135 px en landscape.
- Audio por I2S / speaker.
- Teclado fisico integrado.
