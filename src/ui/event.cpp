namespace event {

void keydown(uint16_t key) {
  if(window_main.focused()) {
    if(key == input_manager.gui.load) load_rom();
    if(key == input_manager.gui.pause) {
      app.pause = !app.pause; //toggle pause state
      if(app.pause) {
        audio.clear();
        if(cartridge.loaded()) update_status();
      }
    }
    if(key == input_manager.gui.reset) reset();
    if(key == input_manager.gui.power) power();
    if(key == input_manager.gui.toggle_fullscreen) toggle_fullscreen();
    if(key == input_manager.gui.toggle_menubar) toggle_menubar();
    if(key == input_manager.gui.toggle_statusbar) toggle_statusbar();
  }

  if(window_input_capture.focused()) {
    if(window_input_capture.waiting == true) {
      if(window_input_capture.locked == false) {
        window_input_capture.assign(key);
      }
    }
  }
}

void keyup(uint16_t key) {
  if(window_input_capture.focused()) {
    if(window_input_capture.waiting == true) {
      if(window_input_capture.locked == true) {
        window_input_capture.locked = input.key_down(keyboard::return_) || input.key_down(keyboard::spacebar);
      }
    }
  }
}

void load_video_settings() {
  video_settings.mode = config::video.mode;
  switch(video_settings.mode) { default:
    case 0: { //windowed
      video_settings.aspect_correction = config::video.windowed.aspect_correction;
      video_settings.synchronize       = config::video.windowed.synchronize;
      video_settings.region            = config::video.windowed.region;
      video_settings.multiplier        = config::video.windowed.multiplier;
      video_settings.hardware_filter   = config::video.windowed.hardware_filter;
      video_settings.software_filter   = config::video.windowed.software_filter;
    } break;

    case 1: { //fullscreen
      video_settings.aspect_correction = config::video.fullscreen.aspect_correction;
      video_settings.synchronize       = config::video.fullscreen.synchronize;
      video_settings.region            = config::video.fullscreen.region;
      video_settings.multiplier        = config::video.fullscreen.multiplier;
      video_settings.hardware_filter   = config::video.fullscreen.hardware_filter;
      video_settings.software_filter   = config::video.fullscreen.software_filter;
    } break;
  }
}

void update_aspect_correction(bool aspect_correction) {
  switch(config::video.mode) { default:
    case 0: config::video.windowed.aspect_correction   = aspect_correction; break;
    case 1: config::video.fullscreen.aspect_correction = aspect_correction; break;
  }
  update_video_settings();
}

void update_multiplier(uint multiplier) {
  switch(config::video.mode) { default:
    case 0: config::video.windowed.multiplier   = multiplier; break;
    case 1: config::video.fullscreen.multiplier = multiplier; break;
  }
  update_video_settings();
}

void update_region(uint region) {
  switch(config::video.mode) { default:
    case 0: config::video.windowed.region   = region; break;
    case 1: config::video.fullscreen.region = region; break;
  }
  update_video_settings();
}

void update_hardware_filter(uint hardware_filter) {
  switch(config::video.mode) { default:
    case 0: config::video.windowed.hardware_filter   = hardware_filter; break;
    case 1: config::video.fullscreen.hardware_filter = hardware_filter; break;
  }
  update_video_settings();
}

void update_software_filter(uint software_filter) {
  switch(config::video.mode) { default:
    case 0: config::video.windowed.software_filter   = software_filter; break;
    case 1: config::video.fullscreen.software_filter = software_filter; break;
  }
  update_video_settings();
}

void update_speed_regulation(uint speed) {
  config::system.speed_regulation = speed <= 5 ? speed : 0;

  //adjust audio frequency to match selected speed setting
  if(audio.cap(Audio::Frequency)) {
    switch(config::system.speed_regulation) {
      case 0: audio.set(Audio::Frequency, 32000); break; //disabled
      case 1: audio.set(Audio::Frequency, 16000); break; //slowest (50%)
      case 2: audio.set(Audio::Frequency, 24000); break; //slow (75%)
      case 3: audio.set(Audio::Frequency, 32000); break; //normal (100%)
      case 4: audio.set(Audio::Frequency, 48000); break; //fast (150%)
      case 5: audio.set(Audio::Frequency, 64000); break; //fastest (200%)
    }
  }

  //do not regulate speed when speed regulation is disabled
  if(audio.cap(Audio::Synchronize)) {
    audio.set(Audio::Synchronize, config::system.speed_regulation != 0);
  }
}

void update_status() {
  if(!cartridge.loaded()) {
    window_main.status.set_text("");
  } else if(app.pause || app.autopause) {
    window_main.status.set_text("(paused)");
  } else if(ppu.status.frames_updated) {
    ppu.status.frames_updated = false;

    unsigned max_framerate = snes.region() == SNES::NTSC ? 60 : 50;
    switch(config::system.speed_regulation) {
      case 0: max_framerate = 0; break;
      case 1: max_framerate = unsigned(0.50 * max_framerate); break;
      case 2: max_framerate = unsigned(0.75 * max_framerate); break;
      case 3: break;
      case 4: max_framerate = unsigned(1.50 * max_framerate); break;
      case 5: max_framerate = unsigned(2.00 * max_framerate); break;
    }

    string output = (const char*)config::misc.status_text;
    replace(output, "%f", string() << (int)ppu.status.frames_executed);
    replace(output, "%m", string() << (int)max_framerate);
    replace(output, "%n", cartridge.info.filename);
    replace(output, "%t", cartridge.info.name);
    window_main.status.set_text(output);
  }
}

void update_video_settings() {
  load_video_settings();

  uint width  = 256;
  uint height = video_settings.region == 0 ? 224 : 239;
  uint multiplier = minmax<1, 5>(video_settings.multiplier);
  width  *= multiplier;
  height *= multiplier;
  if(video_settings.aspect_correction == true) {
    if(video_settings.region == 0) { //NTSC
      width = uint( double(width) * double(config::video.aspect_ntsc_x) / double(config::video.aspect_ntsc_y) );
    } else { //PAL
      width = uint( double(width) * double(config::video.aspect_pal_x)  / double(config::video.aspect_pal_y)  );
    }
  }

  switch(video_settings.mode) { default:
    case 0: { //windowed
      window_main.unfullscreen();
      //try and clamp windows larger than the screen to the screen size.
      //note that since APIs such as X11 lack the ability to get the full window size
      //(with border, etc), this can never be a perfect fit to the screen.
      int w = min(width,  hiro().screen_width());
      int h = min(height, hiro().screen_height());
      window_main.resize(w, h);
      window_main.move(window_main.view, 0, 0);
      window_main.view.resize(w, h);
    } break;

    case 1: { //fullscreen
      window_main.fullscreen();
      int view_width  = window_main.get_width();
      int view_height = window_main.get_height();
      window_main.move(window_main.view,
        //center view inside window, do not exceed window size
        width  < view_width  ? (view_width  - width)  / 2 : 0,
        height < view_height ? (view_height - height) / 2 : 0
      );
      window_main.view.resize(min(width, view_width), min(height, view_height));
    } break;
  }

  libfilter::FilterInterface::FilterType filter;
  switch(video_settings.software_filter) { default:
    case 0: filter = libfilter::FilterInterface::Direct;   break;
    case 1: filter = libfilter::FilterInterface::Scanline; break;
    case 2: filter = libfilter::FilterInterface::Scale2x;  break;
    case 3: filter = libfilter::FilterInterface::HQ2x;     break;
    case 4: filter = libfilter::FilterInterface::NTSC;     break;
  }

  libfilter::filter.set(filter);

  SNES::Video::Mode mode;
  switch(video_settings.region) { default:
    case 0: mode = SNES::Video::ModeNTSC; break;
    case 1: mode = SNES::Video::ModePAL;  break;
  }

  snes.video.set_mode(mode);

  video.set(Video::Synchronize, video_settings.synchronize);
  video.set(Video::Filter, video_settings.hardware_filter);

  //update main window video mode checkbox settings
  window_main.update_menu_settings();
}

void update_opacity() {
  //convert opacity from 50-100 (percentage) to 128-255 (8-bit alpha)
  unsigned opacity = max(50, min(100, config::misc.opacity));
  opacity = unsigned(256.0 / 100.0 * opacity);
  opacity = max(128, min(255, opacity));

  window_settings.set_opacity(opacity);
  window_input_capture.set_opacity(opacity);
  window_bsxloader.set_opacity(opacity);
  window_stloader.set_opacity(opacity);
  window_about.set_opacity(opacity);
}

void toggle_fullscreen() {
  if(config::video.mode != 1) { //switch to fullscreen mode if not already in it
    config::video.mode = 1;
  } else { //switch to windowed mode if already in fullscreen mode
    config::video.mode = 0;
  }
  update_video_settings();
}

void toggle_menubar() {
  window_main.menu.show(!window_main.menu.visible());
  update_video_settings();
}

void toggle_statusbar() {
  window_main.status.show(!window_main.status.visible());
  update_video_settings();
}

//

bool load_rom(char *fn) {
  audio.clear();

  lstring dir;
  strcpy(fn, "");
  strcpy(dir[0], config::path.rom);
  replace(dir[0], "\\", "/");
  if(strlen(dir[0]) && !strend(dir[0], "/")) strcat(dir[0], "/");

  //append base path if rom path is relative
  if(strbegin(dir[0], "./")) {
    ltrim(dir[0], "./");
    strcpy(dir[1], dir[0]);
    strcpy(dir[0], config::path.base);
    strcat(dir[0], dir[1]);
  }

  return hiro().file_open(0, fn,
    dir[0],
    "SNES images;*.smc,*.sfc,*.swc,*.fig,*.bs,*.st"
    #if defined(GZIP_SUPPORT)
    ",*.gz,*.z,*.zip"
    #endif
    #if defined(JMA_SUPPORT)
    ",*.jma"
    #endif
    "|All files;*.*"
  );
}

void load_rom() {
  char fn[PATH_MAX];
  if(load_rom(fn) == false) return;
  load_cart_normal(fn);
}

void load_cart_normal(const char *filename) {
  if(!filename || !*filename) return;

  if(cartridge.loaded() == true) cartridge.unload();
  cartridge.load_cart_normal(filename);

//warn if unsupported hardware detected
  if(cartridge.info.superfx) alert("Warning: unsupported SuperFX chip detected.");
  if(cartridge.info.sa1)     alert("Warning: unsupported SA-1 chip detected.");
  if(cartridge.info.st011)   alert("Warning: unsupported ST011 chip detected.");
  if(cartridge.info.st018)   alert("Warning: unsupported ST018 chip detected.");

  app.pause = false;
  snes.power();
  window_cheat_editor.refresh();
}

void load_cart_bsx(const char *base, const char *slot) {
  if(!base || !*base) return;

  if(cartridge.loaded() == true) cartridge.unload();
  cartridge.load_cart_bsx(base, slot);

  app.pause = false;
  snes.power();
  window_cheat_editor.refresh();
}

void load_cart_bsc(const char *base, const char *slot) {
  if(!base || !*base) return;

  if(cartridge.loaded() == true) cartridge.unload();
  cartridge.load_cart_bsc(base, slot);

  app.pause = false;
  snes.power();
  window_cheat_editor.refresh();
}

void load_cart_st(const char *base, const char *slotA, const char *slotB) {
  if(!base || !*base) return;

  if(cartridge.loaded() == true) cartridge.unload();
  cartridge.load_cart_st(base, slotA, slotB);

  app.pause = false;
  snes.power();
  window_cheat_editor.refresh();
}

void unload_rom() {
  if(cartridge.loaded() == true) {
    cartridge.unload();
    video.clear();
    audio.clear();
  }
  event::update_status();
  window_cheat_editor.refresh();
}

void reset() {
  if(cartridge.loaded() == true) {
    snes.reset();
    dprintf("* Reset");
  }
}

void power() {
  if(cartridge.loaded() == true) {
    snes.power();
    dprintf("* Power");
  }
}

};