# Iteracion 1: una voz polifonica

## Objetivo

Construir un sintetizador real en tiempo real sobre M5Stack Cardputer ADV. Las
notas se generan matematicamente en el dispositivo y se envian al sistema de
audio.

La primera iteracion no intenta ser un sintetizador completo. Debe validar la
cadena base:

```text
teclado fisico
-> eventos note on/off
-> motor de sintesis
-> osciladores matematicos
-> mezcla de hasta 8 notas
-> volumen maestro
-> buffer de audio
-> salida I2S / speaker
```

## Alcance incluido

- Una sola voz/instrumento.
- Polifonia maxima de 8 notas simultaneas.
- Generacion matematica de formas de onda.
- Formas de onda: senoidal, cuadrada, rectangular con duty fijo y diente de
  sierra.
- Lectura del teclado del Cardputer.
- Mapeo de teclas fisicas a notas musicales.
- Cambio de forma de onda con `Fn + 1`, `Fn + 2`, `Fn + 3`, `Fn + 4`.
- Volumen maestro.
- Feedback visual de teclas pulsadas.
- Indicador de polifonia actual.
- Deteccion basica de acordes.
- UI compacta en una sola pantalla.
- Nombre visible de aplicacion: `pocketsynth`, pequeno en la zona superior.
- Iconos pequenos de forma de onda junto a los selectores.

## Fuera de alcance

- ADSR.
- LFO.
- Filtros.
- Multiples canales o capas sonoras.
- Presets.
- Efectos.
- Secuenciador.
- Grabacion.
- Arpegiador.
- Menus profundos.
- Edicion avanzada de parametros.

Estas exclusiones protegen la estabilidad real-time del motor.

## Concepto sonoro

En esta iteracion "voz" significa un unico instrumento o capa sonora. Dentro de
esa voz puede haber hasta 8 notas simultaneas:

```text
Voice 1
|- nota activa 1
|- nota activa 2
|- nota activa 3
|- ...
`- nota activa 8
```

Cada nota activa mantiene su propia fase. No debe existir una unica variable
global de tiempo para todas las notas.

## Estado por nota

```cpp
struct ActiveNote {
  bool active;
  float frequency;
  float phase;
  float phaseIncrement;
};
```

La fase usa rango normalizado:

```text
phase = 0.0 ... 1.0
phaseIncrement = frequency / sampleRate
```

Avance por muestra:

```cpp
phase += phaseIncrement;
if (phase >= 1.0f) {
  phase -= 1.0f;
}
```

Ejemplo: A4 a 440 Hz con `sampleRate = 22050` produce
`phaseIncrement = 440 / 22050 = 0.01995`.

## Formas de onda

Senoidal:

```cpp
sample = sinf(phase * 2.0f * PI);
```

Cuadrada:

```cpp
sample = phase < 0.5f ? 1.0f : -1.0f;
```

Rectangular con duty fijo inicial de 25%:

```cpp
sample = phase < pulseWidth ? 1.0f : -1.0f;
```

Diente de sierra:

```cpp
sample = 2.0f * phase - 1.0f;
```

## Mezcla y normalizacion

Rango interno recomendado: `-1.0 ... +1.0`.

Estrategia inicial:

```text
mixed = sum(activeNotes)
mixed *= perNoteGain
mixed /= sqrt(activeNoteCount)
mixed *= masterVolume
mixed = clamp(mixed, -1.0f, 1.0f)
```

Constantes iniciales:

```cpp
constexpr int MAX_POLYPHONY = 8;
constexpr float PER_NOTE_GAIN = 0.45f;
float masterVolume = 0.70f;
```

Render conceptual:

```cpp
float renderSample(SynthState& state) {
  float mixed = 0.0f;
  int activeCount = 0;

  for (auto& note : state.notes) {
    if (!note.active) continue;

    float s = oscillatorSample(note.phase, state.waveform);
    mixed += s * PER_NOTE_GAIN;

    note.phase += note.phaseIncrement;
    if (note.phase >= 1.0f) {
      note.phase -= 1.0f;
    }

    activeCount++;
  }

  if (activeCount > 1) {
    mixed /= sqrtf((float)activeCount);
  }

  mixed *= state.masterVolume;
  if (mixed > 1.0f) mixed = 1.0f;
  if (mixed < -1.0f) mixed = -1.0f;
  return mixed;
}
```

## Frecuencia de muestreo y buffer

Valores de arranque:

```cpp
constexpr int SAMPLE_RATE = 22050;
constexpr int AUDIO_BUFFER_FRAMES = 128;
constexpr int MAX_POLYPHONY = 8;
```

Motivos:

- 22050 Hz reduce carga de CPU.
- 128 frames ofrece latencia razonable y margen de estabilidad.
- 8 notas cubren acordes complejos en el teclado del Cardputer.

Cuando el sistema sea estable se puede evaluar `SAMPLE_RATE = 32000`.

## Controles

Teclas blancas:

| Tecla | Nota |
| --- | --- |
| z | C4 |
| x | D4 |
| c | E4 |
| v | F4 |
| b | G4 |
| n | A4 |
| m | B4 |
| q | C5 |
| w | D5 |
| e | E5 |
| r | F5 |
| t | G5 |
| y | A5 |
| u | B5 |
| i | C6 |

Teclas negras:

| Tecla | Nota |
| --- | --- |
| s | C#4 / Db4 |
| d | D#4 / Eb4 |
| g | F#4 / Gb4 |
| h | G#4 / Ab4 |
| j | A#4 / Bb4 |
| 2 | C#5 / Db5 |
| 3 | D#5 / Eb5 |
| 5 | F#5 / Gb5 |
| 6 | G#5 / Ab5 |
| 7 | A#5 / Bb5 |

Formas de onda:

| Combinacion | Forma |
| --- | --- |
| Fn + 1 | Senoidal |
| Fn + 2 | Cuadrada |
| Fn + 3 | Rectangular |
| Fn + 4 | Diente de sierra |

Volumen inicial:

| Combinacion | Accion |
| --- | --- |
| Fn + Up | Subir volumen |
| Fn + Down | Bajar volumen |

La implementacion debe usar el mapeo real que exponga la libreria de teclado.

## UI esperada

La pantalla solo debe mostrar lo que existe en esta fase:

- `pocketsynth`.
- `0/8`.
- `CHORD --`.
- `VOL`.
- Selectores `Fn1`, `Fn2`, `Fn3`, `Fn4` con iconos de onda.
- Preview de onda seleccionada.
- Preview de salida.
- Piano.

No debe mostrar mixer, multiples voices, ADSR, LFO, filtro ni presets.

Feedback visual de tecla activa:

| Tipo | Fill | Stroke |
| --- | --- | --- |
| Blanca | `#d8ecff` | `#7cc7ff` |
| Negra | `#25415f` | `#7cc7ff` |

Este feedback debe depender del estado real de notas activas.

## Deteccion de acordes

La deteccion de acordes no debe bloquear audio.

Entrada: conjunto de notas activas.

Salida: nombre de acorde, por ejemplo:

- `C`
- `Cm`
- `CMaj7`
- `CMaj7#5/C`
- `Gsus4/D`

Primera version recomendada:

1. Convertir notas activas a pitch classes.
2. Calcular nota grave.
3. Probar patrones: mayor, menor, disminuido, aumentado, sus2, sus4, 7,
   Maj7 y m7.
4. Mostrar inversion si la nota grave no coincide con la raiz.

Formato de inversion: `CMaj7/E`.

## Criterio global de exito

La iteracion 1 termina cuando el Cardputer ADV actua como sintetizador
real-time basico: una sola voz polifonica de hasta 8 notas, seleccion de forma
de onda, volumen maestro, feedback visual, deteccion de acorde y audio estable.

Prueba final:

1. Encender `pocketsynth`.
2. Pulsar `z`, `x`, `c` y escuchar notas individuales.
3. Tocar acordes de 3 a 5 notas.
4. Cambiar entre `Fn + 1`, `Fn + 2`, `Fn + 3`, `Fn + 4` mientras suena.
5. Ver que el selector e icono de onda cambian.
6. Subir y bajar volumen.
7. Ver contador de polifonia.
8. Ver acorde detectado.
9. Mantener uso durante varios minutos sin cortes.

## Roadmap posterior

- Iteracion 2: multiples canales/voices, waveform y volumen por canal, mixer.
- Iteracion 3: ADSR por nota.
- Iteracion 4: LFO para vibrato, tremolo, PWM y modulacion.
- Iteracion 5: filtro low-pass, high-pass, band-pass y resonancia basica.
- Iteracion 6: presets, guardado/carga de patches, posible uso de SD.
