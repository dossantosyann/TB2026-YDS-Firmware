/* Host unit test for the duration math behind the progress bar.
   WAV: a fabricated RIFF/WAVE header pushed through the wav backend must report exact
   rate/bits and an exact duration (data bytes / byte-rate). It also checks the frames->ms
   identity the audio pipeline uses for elapsed time: pipeline.c itself needs FreeRTOS, so the
   arithmetic contract is asserted here and the live counter is verified on hardware.
   No ESP-IDF, no decoder facade (only the wav backend is linked). */
#include "decoder_backend.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void w32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static void w16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }

/* Canonical 44-byte PCM WAV header + data_bytes of silence in a temp file. */
static FILE *make_wav(uint32_t rate, uint16_t ch, uint16_t bits, uint32_t data_bytes)
{
    FILE *f = tmpfile();
    assert(f);
    uint8_t h[44] = {0};
    memcpy(h, "RIFF", 4);     w32(h + 4, 36 + data_bytes);  memcpy(h + 8, "WAVE", 4);
    memcpy(h + 12, "fmt ", 4); w32(h + 16, 16); w16(h + 20, 1); w16(h + 22, ch);
    w32(h + 24, rate); w32(h + 28, rate * ch * (bits / 8));
    w16(h + 32, (uint16_t)(ch * (bits / 8))); w16(h + 34, bits);
    memcpy(h + 36, "data", 4); w32(h + 40, data_bytes);
    fwrite(h, 1, 44, f);
    for (uint32_t i = 0; i < data_bytes; i++) fputc(0, f);
    rewind(f);
    return f;
}

/* Mirrors pipeline_get_position(): elapsed_ms = played frames / sample rate. */
static uint32_t frames_to_ms(uint32_t frames, uint32_t rate)
{
    return (uint32_t)((uint64_t)frames * 1000u / rate);
}

int main(void)
{
    decoder_format_t fmt;

    /* 1 s of 44.1 kHz / 16-bit / stereo: data = 44100*4 -> duration 1000 ms exactly. */
    FILE *f = make_wav(44100, 2, 16, 44100 * 4);
    assert(decoder_wav_backend.open(f, &fmt) == ESP_OK);
    assert(fmt.rate_hz == 44100 && fmt.bits == 16 && fmt.channels == 2);
    assert(fmt.duration_ms == 1000);
    decoder_wav_backend.close();
    fclose(f);

    /* 0.5 s of 44.1 kHz / 24-bit / mono: byte-rate uses the SOURCE format -> 500 ms. */
    f = make_wav(44100, 1, 24, 22050 * 3);
    assert(decoder_wav_backend.open(f, &fmt) == ESP_OK);
    assert(fmt.bits == 24 && fmt.duration_ms == 500);
    decoder_wav_backend.close();
    fclose(f);

    /* elapsed = frames / rate (the pipeline's per-chunk accumulator). */
    assert(frames_to_ms(44100, 44100) == 1000);
    assert(frames_to_ms(22050, 44100) == 500);
    assert(frames_to_ms(0, 44100) == 0);

    printf("decoder_test: OK\n");
    return 0;
}
