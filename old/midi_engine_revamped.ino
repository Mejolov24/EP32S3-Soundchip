#include <pgmspace.h>
#include <Ticker.h>
#include "sample_0.h"
#include "sample_1.h"
#include "sample_02.h"
#include "sample_3.h"
#include "sample_4.h"
#include "sample_5.h"
#include "sample_6.h"
#include "hit_hat.h"
#include "snare.h"
#include "tomb.h"
Ticker ticker;
const int8_t voices_amount = 32;
volatile int graph_master = 0;
uint8_t active_channels_count = 0;
int playback[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0};
int channels[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
bool active_channels[16] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
bool looping_channels[16] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
float channel_pitch_bend[16] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
int8_t bend_range = 2;

// Estructura de cada voz
struct Voice {
  const int8_t* sample;
  int32_t sample_length;
  uint8_t note;
  uint32_t index;
  uint32_t step;
  uint8_t volume;
  uint8_t channel;
  bool looping;
  bool active;
};


Voice voices[voices_amount];


byte midiCommand = 0;
byte midiChannel = 0;
byte midiNote = 0;
byte midiVelocity = 0;
byte midiInstrument = 0;
byte dataCount;
byte firstByte;
// Flags para eventos únicos
bool noteOnTriggered = false;
bool noteOffTriggered = false;
bool instrumentChanged = false;
// Función para leer datos MIDI desde Serial

void handleNoteOn(byte channel, byte note, byte velocity){
  float note_ratio = pow(2.0, (note - 69) / 12.0);
  float current_bend = (channel == 9) ? 1.0f : channel_pitch_bend[channel];
  uint32_t step = (uint32_t)(note_ratio * 1024.0f * current_bend);
  bool looping = false;
  int32_t vsample_length;
  const int8_t* vsample;

  if (channel == 9){
    step = 1024;
    switch(note){
      case 46:
        vsample = sample_hit;
        vsample_length = sample_hit_l;
      break;
      case 38:
        vsample = sample_snare;
        vsample_length = sample_snare_l;
      break;
      case 41:
        vsample = sample_tomb;
        vsample_length = sample_tomb_l;
      break;
      default:
        vsample = sample_tomb;
        vsample_length = sample_tomb_l;
      break;
    }

  }
  else{

        switch (channels[channel]){ // future me: please change this to be an array this is so unnecesary lmao
  case 0:
    vsample = sample_0_data;
    vsample_length = sample_0_length;
    break;
  case 1:
    vsample = sample_1_data;
    vsample_length = sample_1_length;
    break;
  case 2:
    vsample = sample_2_data;
    vsample_length = sample_2_length;
    break;
  case 3:
    vsample = sample_3_data;
    vsample_length = sample_3_length;
    break;
  case 4:
    vsample = sample_4_data;
    vsample_length = sample_4_length;
    break;
  case 5:
    vsample = sample_5_data;
    vsample_length = sample_5_length;
    break;
  case 6:
    vsample = sample_6_data;
    vsample_length = sample_6_length;
    break;
  }
  }
  startVoice(vsample,vsample_length,note,step,velocity / 2,channel,looping);

}

void handleNoteOff(byte note) {
    for (int i = 0; i < voices_amount ; i++){
      if (voices[i].active){
      if (voices[i].active && voices[i].note == note) {voices[i].active = false;}
  
    }
    }
  }

void readMIDI() {
  while (Serial.available() > 0) {
    byte incoming = Serial.read();

    if (incoming >= 0xF8) continue; 

    if (incoming >= 0x80) { 
      midiCommand = incoming & 0xF0;
      midiChannel = incoming & 0x0F;
      dataCount = 0; 
    } 
    else { 
      if (dataCount == 0) {
        firstByte = incoming;
        dataCount = 1;
        
        // Mensajes de 1 solo byte de datos (Program Change)
        if (midiCommand == 0xC0) { 
          channels[midiChannel] = firstByte; 
          dataCount = 0; 
        }
      } else {
        byte secondByte = incoming;
        dataCount = 0;

        // --- MANEJO DE NOTE ON ---
        if (midiCommand == 0x90) {
          if (secondByte > 0) handleNoteOn(midiChannel, firstByte, secondByte);
          else handleNoteOff(firstByte);
        } 
        // --- MANEJO DE NOTE OFF ---
        else if (midiCommand == 0x80) {
          handleNoteOff(firstByte);
        }
        // --- MANEJO DE PITCH BEND (0xE0) ---
        else if (midiCommand == 0xE0) {
          // El Pitch Bend usa 14 bits: el primer byte son los 7 bits bajos, el segundo los 7 altos.
          int bendRaw = (secondByte << 7) | firstByte; 
          
          // Convertimos 0..16383 a un rango de semitonos (-2.0 a +2.0)
          float semitones = ((float)bendRaw - 8192.0f) / 8192.0f * bend_range;
          
          // Calculamos el multiplicador y lo guardamos para el canal
          channel_pitch_bend[midiChannel] = pow(2.0, semitones / 12.0);

          // ACTUALIZACIÓN EN TIEMPO REAL:
          // Recorremos las voces para aplicar el bend a las notas que ya están sonando
          for (int i = 0; i < voices_amount; i++) {
            if (voices[i].active && voices[i].channel == midiChannel) {
              // Si no es el canal de percusión, recalculamos el step
              if (voices[i].channel != 9) {
                float note_ratio = pow(2.0, (voices[i].note - 69) / 12.0);
                voices[i].step = (uint32_t)(note_ratio * 1024.0f * channel_pitch_bend[midiChannel]);
              }
            }
          }
        }
      }
    }
  }
}

// Iniciar una nueva voz con un sample
void startVoice( const int8_t* sample,int32_t sample_length, int8_t note, uint32_t step, int8_t volume,int8_t channel , bool looping) {
  for (int i = 0; i < voices_amount ; i++) {
    if (!voices[i].active) {
      voices[i].note = note;
      voices[i].sample = sample;
      voices[i].sample_length = sample_length;
      voices[i].active = true;
      voices[i].index = 0;
      voices[i].step = step;
      voices[i].volume = volume;
      voices[i].channel = channel;
      voices[i].looping = looping;
      return;
    }
  }
}



int Mix_channel(int channel){
  uint8_t active_voices = 0;
  int32_t mix = 0;
  int32_t sum = 0;
for (int i = 0; i < voices_amount ; i++){
if (voices[i].active && voices[i].channel == channel ){
  active_voices ++;
  uint32_t index = voices[i].index >> 10;
  int16_t sample = (int8_t)pgm_read_byte(&(voices[i].sample[index]));
  if (voices[i].index < voices[i].sample_length * 1024){
    voices[i].index += voices[i].step;
    sum += (sample * voices[i].volume) / 128;
  }
  else{
    if (voices[i].looping){voices[i].index = 0;}
    else{voices[i].active = false;}
  }
}
}
    if (active_voices == 0){return 0;}
    mix = sum;
    mix = constrain(mix,-127,128);
    return mix;
}

void  Handle_synthesis(){
  int16_t mix = 0;
  int32_t sum = 0;
  active_channels_count = 0;
  for (int i = 0; i < 16; i++) active_channels[i] = false;
    for (int i = 0; i < voices_amount; i++) {
        if (voices[i].active) {
            active_channels[voices[i].channel] = true;
        }
    }
    for (int i = 0; i < 16; i++) {
        if (active_channels[i]) active_channels_count++;
    }

for (int i = 0; i < 16; i++){
     playback[i] = Mix_channel(i);
    sum += playback[i];
}
    mix = sum;
    mix = constrain(mix,-127,128);
    playback[16] = mix;
    ledcWrite(5,mix + 128);
}

hw_timer_t *timer = NULL;
volatile bool sendFlag = false;
void IRAM_ATTR sendSample() {
  sendFlag = true;
}

void setup() {
  Serial.setRxBufferSize(1024);
  Serial.begin(1000000);
  for (int i = 0; i < voices_amount ; i++) voices[i].active = false;
  ledcAttach(5,80000,8);
  ticker.attach_us(45, Handle_synthesis);
  timer = timerBegin(5000);
  timerAttachInterrupt(timer, &sendSample);
  timerAlarm(timer, 1, true, 0); 
}

void loop() {
  readMIDI();

  if (sendFlag) {
    sendFlag = false;
    for(int i = 0; i < 17; i++){
    Serial.write(255);
    Serial.write(i);
    Serial.write(int8_t(playback[i]));
    }
  }
}
