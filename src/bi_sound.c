#include <mruby.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#ifdef __APPLE__
#include <SDL2_mixer/SDL_mixer.h>
#elif __EMSCRIPTEN__
#include <emscripten.h>
#include <SDL/SDL_mixer.h> // mixer SDL1, not 2
#else
#include <SDL_mixer.h>
#endif


// Bi::Sound class
static struct mrb_data_type const mrb_sound_data_type = { "Sound", mrb_free };

//
// class methods
//

static mrb_value mrb_sound_class_init(mrb_state *mrb, mrb_value self)
{
  mrb_int freq, channel, buffer;
  mrb_get_args(mrb, "iii", &freq, &channel, &buffer );

  Mix_Init(0);
  if( Mix_OpenAudio( freq, MIX_DEFAULT_FORMAT, channel, buffer ) == -1 ) {
    printf("Mix_OpenAudio fail\n");
  }

#ifndef __EMSCRIPTEN__
  // get and print the audio format in use
  int numtimesopened, frequency, channels;
  Uint16 format;
  numtimesopened=Mix_QuerySpec(&frequency, &format, &channels);
  if(!numtimesopened) {
    printf("Mix_QuerySpec: %s\n",Mix_GetError());
  }
  else {
    const char *format_str="Unknown";
    switch(format) {
        case AUDIO_U8: format_str="U8"; break;
        case AUDIO_S8: format_str="S8"; break;
        case AUDIO_U16LSB: format_str="U16LSB"; break;
        case AUDIO_S16LSB: format_str="S16LSB"; break;
        case AUDIO_U16MSB: format_str="U16MSB"; break;
        case AUDIO_S16MSB: format_str="S16MSB"; break;
    }
    // printf("opened=%d times  frequency=%dHz  format=%s  channels=%d",
    //         numtimesopened, frequency, format_str, channels);

    mrb_iv_set(mrb, self, mrb_intern_cstr(mrb,"@@format"), mrb_str_new_cstr(mrb,format_str) );
    mrb_iv_set(mrb, self, mrb_intern_cstr(mrb,"@@frequency"), mrb_fixnum_value(frequency) );
    mrb_iv_set(mrb, self, mrb_intern_cstr(mrb,"@@channels"), mrb_fixnum_value(channels) );
  }
#endif

  return self;
}

static mrb_value mrb_sound_class_allocate_channels(mrb_state *mrb, mrb_value self)
{
  mrb_int channel;
  mrb_get_args(mrb, "i", &channel );
  return mrb_fixnum_value( Mix_AllocateChannels(channel) );
}

//
// instance methods
//

static mrb_value mrb_sound_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value name;
  mrb_get_args(mrb, "S", &name );

  const char* filename = mrb_string_value_cstr(mrb,&name);

  SDL_RWops *rw = SDL_RWFromFile( filename, "rb");
  if (NULL == rw) {
      printf("SDL_RWFromFile: %s\n", SDL_GetError());
      return self;
  }

  Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 1);
  if (chunk == NULL) {
    printf("Mix_LoadWAV fail %s\n",filename);
    return self;
  }

  DATA_PTR(self) = chunk;
  DATA_TYPE(self) = &mrb_sound_data_type;

  return self;
}

static mrb_value mrb_sound_play(mrb_state *mrb, mrb_value self)
{
  mrb_int channel, loop;
  mrb_get_args(mrb, "ii", &channel, &loop );

  Mix_Chunk *chunk = DATA_PTR(self);

  if ( Mix_PlayChannel( channel, chunk, loop) == -1 ) {
    printf("Mix_PlayChannel fail\n");
  }

  return self;
}

void mrb_mruby_bi_sound_gem_init(mrb_state* mrb)
{
  struct RClass *bi;
  bi = mrb_define_class(mrb, "Bi", mrb->object_class);
  MRB_SET_INSTANCE_TT(bi, MRB_TT_DATA);

  struct RClass *sound;
  sound = mrb_define_class_under(mrb, bi, "Sound", mrb->object_class);
  MRB_SET_INSTANCE_TT(sound, MRB_TT_DATA);

  mrb_define_class_method(mrb, sound, "init", mrb_sound_class_init, MRB_ARGS_REQ(4));
  mrb_define_class_method(mrb, sound, "allocate_channels", mrb_sound_class_allocate_channels, MRB_ARGS_REQ(1));

  mrb_define_method(mrb, sound, "initialize", mrb_sound_initialize, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, sound, "play", mrb_sound_play, MRB_ARGS_REQ(2));
}

void mrb_mruby_bi_sound_gem_final(mrb_state* mrb)
{
}
