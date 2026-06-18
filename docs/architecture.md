# Arquitectura runtime

## Regla principal

El audio manda. La generacion de audio nunca debe esperar por UI, logs,
teclado, SD, graficos o locks largos.

## Tareas FreeRTOS propuestas

| Tarea | Prioridad | Responsabilidad |
| --- | --- | --- |
| `AudioTask` | Alta | Renderizar buffers y escribir a I2S/speaker. |
| `InputTask` | Media | Escanear teclado y emitir eventos de cambio. |
| `SynthControlTask` | Media | Aplicar note on/off, waveform, volumen y estado. |
| `UiTask` | Baja | Redibujar estado visible solo cuando cambie. |
| `ChordTask` | Baja/media | Recalcular acorde al cambiar notas activas. |

`ChordTask` puede integrarse inicialmente en `SynthControlTask`.

## Flujo de datos

```text
Cardputer keyboard
  -> InputTask
  -> SynthEvent queue
  -> SynthControlTask
  -> SynthState pequeno y copiable
  -> AudioTask render
  -> I2S / speaker

SynthControlTask
  -> UiState
  -> UiTask
  -> Cardputer display
```

## Eventos de entrada

```cpp
enum class EventType {
  NoteOn,
  NoteOff,
  SetWaveform,
  SetVolume
};

struct SynthEvent {
  EventType type;
  uint8_t key;
  uint8_t noteIndex;
  float value;
};
```

`InputTask` debe enviar eventos sin bloquear:

```cpp
xQueueSend(synthEventQueue, &event, 0);
```

## Estado del sintetizador

```cpp
enum class Waveform {
  Sine,
  Square,
  Rectangle,
  Saw
};

struct ActiveNote {
  bool active;
  float frequency;
  float phase;
  float phaseIncrement;
};

struct SynthState {
  Waveform waveform;
  float masterVolume;
  ActiveNote notes[MAX_POLYPHONY];
  uint32_t pressedMask;
};
```

La iteracion 1 puede empezar con copia de estado pequeno y seccion critica
corta. Si aparecen glitches, pasar a doble estado:

```text
controlState -> copia atomica/corta -> audioState
```

## Reglas del AudioTask

Dentro del camino de audio no debe haber:

- `printf` o `ESP_LOG*`.
- `malloc`, `new`, `std::vector` dinamico o `String`.
- Lectura de SD.
- Redibujado de pantalla.
- Espera de mutex largo.
- Operaciones I2C lentas.

Estructura esperada:

```cpp
while (true) {
  renderAudioBuffer(buffer, AUDIO_BUFFER_FRAMES);
  i2s_write(...);
}
```

## Render y salida

- Formato interno: `float` normalizado en `[-1.0, 1.0]`.
- Salida final: `int16`.
- Sample rate inicial: 22050 Hz.
- Buffer inicial: 128 frames.
- Polifonia maxima: 8 notas.

Conversion conceptual:

```cpp
int16_t toInt16(float sample) {
  if (sample > 1.0f) sample = 1.0f;
  if (sample < -1.0f) sample = -1.0f;
  return (int16_t)(sample * 32767.0f);
}
```

## Politica de polifonia

Para exceder 8 notas en iteracion 1:

- Ignorar nuevas notas hasta que se libere alguna activa.

Alternativa futura:

- Voice stealing.

## UI y estado visual

La UI nunca debe derivar notas pulsadas solo de eventos momentaneos. Debe leer
un estado estable generado por control:

- Notas activas.
- Waveform activa.
- Volumen maestro.
- Conteo `n/8`.
- Nombre de acorde.

Frecuencia objetivo de UI:

- 15-20 FPS maximo.
- Redibujar solo si hay cambios.
