import mido
import sys
import os

def split_midi_channels(input_file):
    # Generar el nombre de salida con el sufijo _split
    base, ext = os.path.splitext(input_file)
    output_file = f"{base}_split{ext}"

    mid = mido.MidiFile(input_file)
    new_mid = mido.MidiFile(ticks_per_beat=mid.ticks_per_beat)

    for i, track in enumerate(mid.tracks):
        new_track = mido.MidiTrack()
        new_mid.tracks.append(new_track)
        
        # Diccionario para rastrear notas activas: {nota: canal_asignado}
        active_notes = {}
        
        for msg in track:
            if msg.is_meta:
                new_track.append(msg)
                continue

            if msg.type == 'note_on' and msg.velocity > 0:
                # Buscar primer canal libre (0-15), omitiendo el 9 (Canal 10 MIDI)
                used_channels = set(active_notes.values())
                assigned_channel = 0
                
                for ch in range(16):
                    if ch == 9: # Omitir el canal 10
                        continue
                    if ch not in used_channels:
                        assigned_channel = ch
                        break
                
                active_notes[msg.note] = assigned_channel
                new_msg = msg.copy(channel=assigned_channel)
                new_track.append(new_msg)

            elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                if msg.note in active_notes:
                    assigned_channel = active_notes.pop(msg.note)
                    new_msg = msg.copy(channel=assigned_channel)
                    new_track.append(new_msg)
                else:
                    # Si por alguna razón no estaba en el registro, enviarlo al canal 0
                    new_track.append(msg.copy(channel=0))
            else:
                # Otros mensajes (pedal, pitch bend, etc.) se mantienen en su canal original
                # o podrías forzarlos al canal 0 si prefieres
                new_track.append(msg)

    new_mid.save(output_file)
    print(f"Procesado: {output_file} (Canal 10 omitido)")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        file_path = sys.argv[1]
        if os.path.exists(file_path):
            split_midi_channels(file_path)
        else:
            print("Error: El archivo no existe.")
    else:
        print("Uso: Arrastra un archivo MIDI sobre este script .py")