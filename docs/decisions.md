# Decisiones tecnicas iniciales

| Decision | Valor |
| --- | --- |
| Hardware | M5Stack Cardputer ADV |
| Resolucion UI | 240 x 135 px landscape |
| Voces/canales | 1 |
| Polifonia | 8 notas maximo |
| ADSR | No en iteracion 1 |
| LFO | No en iteracion 1 |
| Filtro | No en iteracion 1 |
| Sample rate inicial | 22050 Hz |
| Buffer inicial | 128 frames |
| Formato interno | `float` |
| Salida final | `int16` |
| UI FPS | 15-20 FPS maximo |
| Nombre visible | `pocketsynth` |
| Selectores de onda | `Fn + 1..4` con iconos compactos |
| Normalizacion | `sqrt(activeNoteCount)` mas headroom |
| Ganancia por nota inicial | `PER_NOTE_GAIN = 0.45f` |
| Volumen maestro inicial | `0.70f` |
| Onda rectangular | Duty fijo inicial de 25% |
| Exceso de polifonia | Ignorar nuevas notas |

## Riesgos principales

### Cortes de audio

Causas probables:

- `AudioTask` bloqueado.
- Buffer demasiado pequeno.
- UI demasiado pesada.
- Logs dentro del audio path.

Mitigaciones:

- Prioridad alta para audio.
- Sin logs ni memoria dinamica en render.
- UI a baja frecuencia.
- Buffer ajustable.

### Clipping

Causas probables:

- Suma de muchas notas.
- Ganancia por nota demasiado alta.
- Volumen maestro alto.

Mitigaciones:

- `PER_NOTE_GAIN`.
- Division por `sqrt(activeNoteCount)`.
- Clamp final.
- Indicador futuro de clipping si hace falta.

### Latencia de teclado

Causas probables:

- `InputTask` lento.
- Debounce excesivo.
- Bloqueos en lectura.

Mitigaciones:

- Escaneo cada 5-10 ms.
- Eventos solo en cambios.
- Separar input de UI.

### UI saturada

Causas probables:

- Demasiada informacion en 240 x 135 px.
- Textos largos.
- Graficas demasiado grandes.

Mitigaciones:

- UI compacta.
- Abreviaturas.
- Mostrar solo lo implementado.
- Evitar conceptos futuros en pantalla.
