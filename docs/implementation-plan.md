# Plan de implementacion

La primera iteracion se divide en fases pequenas y medibles. Cada fase debe
conservar la estabilidad del audio antes de avanzar.

## Fase 1A: audio minimo

Objetivo:

- Inicializar salida de audio.
- Generar una nota fija.
- Confirmar que el buffer no se corta.

Criterio de aceptacion:

- Suena una nota continua sin cortes audibles durante al menos 60 segundos.

Notas de implementacion:

- Usar `SAMPLE_RATE = 22050`.
- Usar `AUDIO_BUFFER_FRAMES = 128`.
- Sin logs en el camino de audio.
- Convertir `float` interno a `int16` antes de I2S.

## Fase 1B: teclado monofonico

Objetivo:

- Leer teclado fisico.
- Mapear una tecla a una nota.
- Generar note on/off.

Criterio de aceptacion:

- Al pulsar `z` suena C4.
- Al soltar `z` deja de sonar.
- El audio no se bloquea.

## Fase 1C: polifonia de 8 notas

Objetivo:

- Permitir hasta 8 notas simultaneas.
- Mantener fase independiente por nota.
- Normalizar la suma.

Criterio de aceptacion:

- Se pueden tocar acordes de 3, 4 y 5 notas sin clipping evidente.
- El contador de polifonia muestra `n/8`.
- Si se excede el maximo, el sistema no se rompe.

Politica inicial al exceder 8 notas:

- Ignorar nuevas notas hasta liberar una activa.

## Fase 1D: seleccion de forma de onda

Objetivo:

- `Fn + 1`: senoidal.
- `Fn + 2`: cuadrada.
- `Fn + 3`: rectangular.
- `Fn + 4`: diente de sierra.
- Mostrar iconos pequenos de forma de onda.

Criterio de aceptacion:

- Se puede cambiar de onda mientras suena una nota o acorde.
- La UI refleja la onda seleccionada mediante estado activo e icono.

## Fase 1E: UI compacta

Objetivo:

- Mostrar estado minimo.
- Pintar teclas activas.
- Mostrar waveform y output preview.
- Mostrar volumen y polifonia.

Criterio de aceptacion:

- La UI se actualiza sin provocar cortes de audio.
- La tasa de refresco es baja pero estable.

Recomendacion:

- 15-20 FPS maximo.
- Redibujar solo si hay cambios.

## Fase 1F: deteccion de acordes

Objetivo:

- Identificar acordes basicos a partir de notas activas.
- Mostrar el nombre del acorde en pantalla.

Criterio de aceptacion:

- Triadas mayores y menores se detectan correctamente.
- Septimas basicas se detectan correctamente.
- Las inversiones muestran bajo cuando procede.

Patrones iniciales:

- Mayor.
- Menor.
- Disminuido.
- Aumentado.
- Sus2.
- Sus4.
- 7.
- Maj7.
- m7.

## Fase 1G: estabilizacion y profiling

Objetivo:

- Medir carga de CPU.
- Comprobar underruns.
- Comprobar latencia.
- Ajustar buffer.
- Ajustar sample rate si procede.

Criterio de aceptacion:

- El sistema permite tocar varios minutos sin cortes, bloqueos ni degradacion.

## Checklist final

- `pocketsynth` arranca y muestra UI compacta.
- `z`, `x`, `c` producen C4, D4, E4.
- Acordes de 3-5 notas suenan sin clipping evidente.
- `Fn + 1..4` cambia forma de onda.
- Volumen maestro sube y baja.
- Contador de polifonia refleja notas activas.
- Acorde detectado aparece en pantalla.
- No hay logs ni asignaciones dinamicas en render de audio.
- Uso prolongado estable durante varios minutos.
