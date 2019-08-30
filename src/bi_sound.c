#include <mruby.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#ifdef __APPLE__
#include <SDL2_mixer/SDL_mixer.h>
#elif __EMSCRIPTEN__
#include <emscripten.h>
#include <SDL/SDL_mixer.h> // SDL1
#else
#include <SDL_mixer.h>
#endif

// Bi::Sound class
static void mrb_bi_sound_free(mrb_state *mrb, void* ptr){ Mix_FreeChunk(ptr); }
static struct mrb_data_type const mrb_bi_sound_data_type = { "Sound", mrb_bi_sound_free };

// Bi::Music class
static void mrb_bi_music_free(mrb_state *mrb, void* ptr){ Mix_FreeMusic(ptr); }
static struct mrb_data_type const mrb_bi_music_data_type = { "Music", mrb_bi_music_free };

//
// class methods
//

static mrb_value mrb_bi_sound_init(mrb_state *mrb, mrb_value self)
{
  mrb_int freq, channel, buffer;
  mrb_get_args(mrb, "iii", &freq, &channel, &buffer );

  Mix_Init(MIX_INIT_MP3|MIX_INIT_OGG);
  if( Mix_OpenAudio( freq, MIX_DEFAULT_FORMAT, channel, buffer ) == -1 ) {
    return mrb_false_value();
  }

#ifndef __EMSCRIPTEN__
  // get and print the audio format in use
  int frequency, channels;
  Uint16 format;
  if( Mix_QuerySpec(&frequency, &format, &channels) > 0 )
  {
    const char *format_str="Unknown";
    switch(format) {
        case AUDIO_U8: format_str="U8"; break;
        case AUDIO_S8: format_str="S8"; break;
        case AUDIO_U16LSB: format_str="U16LSB"; break;
        case AUDIO_S16LSB: format_str="S16LSB"; break;
        case AUDIO_U16MSB: format_str="U16MSB"; break;
        case AUDIO_S16MSB: format_str="S16MSB"; break;
    }
    mrb_iv_set(mrb, self, mrb_intern_cstr(mrb,"@@format"), mrb_str_new_cstr(mrb,format_str) );
    mrb_iv_set(mrb, self, mrb_intern_cstr(mrb,"@@frequency"), mrb_fixnum_value(frequency) );
    mrb_iv_set(mrb, self, mrb_intern_cstr(mrb,"@@channels"), mrb_fixnum_value(channels) );
  }
#endif

  return mrb_true_value();
}

static mrb_value mrb_bi_sound_allocate_channels(mrb_state *mrb, mrb_value self)
{
  mrb_int channel;
  mrb_get_args(mrb, "i", &channel );
  return mrb_fixnum_value( Mix_AllocateChannels(channel) );
}


static mrb_value mrb_bi_sound_read(mrb_state *mrb, mrb_value self)
{
  mrb_value filename;
  mrb_get_args(mrb, "S", &filename );

  const char* path = mrb_string_value_cstr(mrb,&filename);

  SDL_RWops *rw = SDL_RWFromFile(path, "rb");
  if (NULL == rw) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed sound data loading.");
  }

  Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 1);
  if (chunk == NULL) {
    SDL_RWclose(rw);
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed sound data loading.");
  }

  struct RClass *bi = mrb_class_get(mrb, "Bi");
  struct RClass *klass = mrb_class_get_under(mrb,bi,"Sound");
  struct RData *data = mrb_data_object_alloc(mrb,klass,chunk,&mrb_bi_sound_data_type);
  return mrb_obj_value(data);
}

static mrb_value mrb_bi_sound_pan(mrb_state *mrb, mrb_value self)
{
  mrb_int channel,left,right;
  mrb_get_args(mrb, "iii", &channel,&left,&right );

  return mrb_bool_value( Mix_SetPanning(channel,left,right) != 0 );
}

//
// instance methods
//

static mrb_value mrb_bi_sound_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value data;
  mrb_get_args(mrb, "S", &data );

  SDL_RWops *rw = SDL_RWFromConstMem( RSTRING_PTR(data), RSTRING_LEN(data) );
  if (NULL == rw) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed sound data loading.");
  }

  Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 1);
  if (chunk == NULL) {
    SDL_RWclose(rw);
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed sound data loading.");
  }

  DATA_PTR(self) = chunk;
  DATA_TYPE(self) = &mrb_bi_sound_data_type;

  return self;
}

static mrb_value mrb_bi_sound_play(mrb_state *mrb, mrb_value self)
{
  mrb_int channel, loop;
  mrb_get_args(mrb, "ii", &channel, &loop );

  Mix_Chunk *chunk = DATA_PTR(self);

  return mrb_fixnum_value( Mix_PlayChannel( channel, chunk, loop) );
}

//
// Bi::Music
//

static Mix_Music* _load_music(SDL_RWops *rw)
{
#if SDL_MIXER_MAJOR_VERSION == 1
  Mix_Music *music = Mix_LoadMUS_RW(rw);
#else
  Mix_Music *music = Mix_LoadMUS_RW(rw, 1);
#endif
  return music;
}

static mrb_value mrb_bi_music_read(mrb_state *mrb, mrb_value self)
{
  mrb_value filename;
  mrb_get_args(mrb, "S", &filename);

  const char* f = mrb_string_value_cstr(mrb,&filename);

  SDL_RWops *rw = SDL_RWFromFile( f, "rb");
  if (NULL == rw) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed music data loading.");
  }

  Mix_Music *music = _load_music(rw);
  if (music == NULL) {
    SDL_RWclose(rw);
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed music data loading.");
  }

  struct RClass *bi = mrb_class_get(mrb, "Bi");
  struct RClass *klass = mrb_class_get_under(mrb,bi,"Music");
  struct RData *data = mrb_data_object_alloc(mrb,klass,music,&mrb_bi_music_data_type);
  return mrb_obj_value(data);
}

static mrb_value mrb_bi_music_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value data;
  mrb_get_args(mrb, "S", &data );

  SDL_RWops *rw = SDL_RWFromConstMem( RSTRING_PTR(data), RSTRING_LEN(data) );
  if (NULL == rw) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed music data loading.");
  }

  Mix_Music *music = _load_music(rw);
  if (music == NULL) {
    SDL_RWclose(rw);
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed music data loading.");
  }

  DATA_PTR(self) = music;
  DATA_TYPE(self) = &mrb_bi_music_data_type;

  mrb_iv_set(mrb, self, mrb_intern_cstr(mrb,"@_music_data"), data);

  return self;
}

static mrb_value mrb_bi_music_set_volume(mrb_state *mrb, mrb_value self)
{
  mrb_int volume;
  mrb_get_args(mrb, "i", &volume );
  return mrb_fixnum_value( Mix_VolumeMusic(volume) );
}

static mrb_value mrb_bi_music_get_volume(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value( Mix_VolumeMusic(-1) );
}

//
// Bi::Music instance methods
//

static mrb_value mrb_bi_music_play(mrb_state *mrb, mrb_value self)
{
  mrb_int loop;
  mrb_get_args(mrb, "i", &loop );

  Mix_Music *music = DATA_PTR(self);

  return mrb_bool_value( Mix_PlayMusic( music, loop) == 0 );
}

//
// ----
//

void mrb_mruby_bi_sound_gem_init(mrb_state* mrb)
{
  struct RClass *bi;
  bi = mrb_define_class(mrb, "Bi", mrb->object_class);
  MRB_SET_INSTANCE_TT(bi, MRB_TT_DATA);

  struct RClass *sound;
  sound = mrb_define_class_under(mrb, bi, "Sound", mrb->object_class);
  MRB_SET_INSTANCE_TT(sound, MRB_TT_DATA);

  mrb_define_class_method(mrb, sound, "init", mrb_bi_sound_init, MRB_ARGS_REQ(3)); // freq, channel, buffer
  mrb_define_class_method(mrb, sound, "allocate_channels", mrb_bi_sound_allocate_channels, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, sound, "read", mrb_bi_sound_read, MRB_ARGS_REQ(1)); // filename
  mrb_define_class_method(mrb, sound, "pan", mrb_bi_sound_pan, MRB_ARGS_REQ(3)); // channel,left,right

  mrb_define_method(mrb, sound, "initialize", mrb_bi_sound_initialize, MRB_ARGS_REQ(1)); // sound-data
  mrb_define_method(mrb, sound, "play", mrb_bi_sound_play, MRB_ARGS_REQ(2)); // channel, loop

  //

  struct RClass *music;
  music = mrb_define_class_under(mrb, bi, "Music", mrb->object_class);
  MRB_SET_INSTANCE_TT(music, MRB_TT_DATA);

  mrb_define_class_method(mrb, music, "read", mrb_bi_music_read, MRB_ARGS_REQ(1)); // filename
  mrb_define_class_method(mrb, music, "volume=", mrb_bi_music_set_volume, MRB_ARGS_REQ(1)); // volume(0-127)
  mrb_define_class_method(mrb, music, "volume", mrb_bi_music_get_volume, MRB_ARGS_NONE());
  mrb_define_method(mrb, music, "initialize", mrb_bi_music_initialize, MRB_ARGS_REQ(1)); // music-data
  mrb_define_method(mrb, music, "play", mrb_bi_music_play, MRB_ARGS_REQ(1)); // loop
}

void mrb_mruby_bi_sound_gem_final(mrb_state* mrb)
{
}
