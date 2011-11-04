#!/usr/bin/env python
#
# Clutter-Gstreamer
#
# Gstreamer integration library for Clutter.
#
# video-player.py - A simple Python GObject-Introspection port of video-player.c
#
# Copyright (C) 2007,2008 OpenedHand
# Copyright (C) 2011 Kinkrsoftware
#
# Build along the wiki example by Dinh Bowman
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

import sys
import codecs
from gi.repository import Clutter, ClutterGst
from os.path import basename, exists

# An easy way to debug clutter and cogl without having to type the
# command line arguments
#DEBUG = True
DEBUG = False
debugArgs = ['--clutter-debug=all', '--cogl-debug=all']

class Playlist:
    def __init__(self, loop=False):
        self.entries = []
        self.current = -1
        self.loop = loop

    def load(self, filename):
        if filename.lower().endswith('.m3u'):
            self._loadm3u(filename)
        else:
            self.entries.append(filename)

    def _loadm3u(self, filename):
        f = codecs.open(filename, 'r', 'utf-8')
        for line in f.read().split('\n'):
            if len(line) > 1 and line[0] != '#' and exists(line):
                self.entries.append(line)

    def advance(self):
        self.current += 1
        if self.current >= len(self.entries):
            if self.loop:
                self.current = 0
            else:
                self.current -= 1
                return None

        return self.entries[self.current]

class VideoApp:
    def __init__(self, filename, logo=None, controls_visible=True, fullscreen=False, loop=False, anamorph=False):
        """
        Build the user interface.
        """
        stage_color    = Clutter.Color.new(   0,    0,    0,    0)
        control_color1 = Clutter.Color.new(  73,   74,   77, 0xee)
        control_color2 = Clutter.Color.new(0xcc, 0xcc, 0xcc, 0xff)

        self.SEEK_W = 440
        self.SEEK_H = 14

        self.fullscreen = fullscreen

        self.playlist = Playlist(loop)
        self.playlist.load(filename)
        self.anamorph = anamorph

        self.controls_showing = False
        self.paused           = True
        self.controls_timeout = 0

        ClutterGst.init(sys.argv)

        self.stage = Clutter.Stage.get_default()
        self.stage.set_title("A simple video player ported to Python")
        self.stage.set_color(stage_color)
        self.stage.set_size(768, 576)
        self.stage.set_minimum_size(640, 480)

        if self.fullscreen:
            self.stage.set_fullscreen(True)

        self.vtexture = ClutterGst.VideoTexture()

        """
        By default ClutterGst seeks to the nearest key frame (faster). However
        it has the weird effect that when you click on the progress bar, the fil
        goes to the key frame position that can be quite far from where you
        clicked. Using the ACCURATE flag tells playbin2 to seek to the actual
        frame
        """
        self.vtexture.set_seek_flags(ClutterGst.SeekFlags(1))

        self.vtexture.connect("eos", self._on_video_texture_eos)

        self.stage.connect("allocation-changed", self._on_stage_allocation_changed)

        # Handle it ourselves so can scale up for fullscreen better
        self.vtexture.connect_after("size-change", self._size_change)

        # Load up out video texture
        self.vtexture.set_filename(self.playlist.advance())

        # Create the control UI
        self.control = Clutter.Group.new()
        self.control_bg = Clutter.Texture().new_from_file("vid-panel.png")
        self.control_play = Clutter.Texture().new_from_file("media-actions-start.png")
        self.control_pause = Clutter.Texture().new_from_file("media-actions-pause.png")

        self.control_seek1   = Clutter.Rectangle().new_with_color(control_color1)
        self.control_seek2   = Clutter.Rectangle().new_with_color(control_color2)
        self.control_seekbar = Clutter.Rectangle().new_with_color(control_color1)
        self.control_seekbar.set_opacity(0x99)

        self.control_label = Clutter.Text().new_full("Sans Bold 14",
                                                     basename(filename),
                                                     control_color1)

        self.control_play.hide()

        self.control.add_actor(self.control_bg)
        self.control.add_actor(self.control_play)
        self.control.add_actor(self.control_pause)
        self.control.add_actor(self.control_seek1)
        self.control.add_actor(self.control_seek2)
        self.control.add_actor(self.control_seekbar)
        self.control.add_actor(self.control_label)

        self.control.set_opacity(0x99)

        self.control_play.set_position(22, 31)
        self.control_pause.set_position(18, 31)

        self.control_seek1.set_size(self.SEEK_W+4, self.SEEK_H+4)
        self.control_seek1.set_position(80, 57)
        self.control_seek2.set_size(self.SEEK_W, self.SEEK_H)
        self.control_seek2.set_position(82, 59)
        self.control_seekbar.set_size(0, self.SEEK_H)
        self.control_seekbar.set_position(82, 59)

        self.control_label.set_position(82, 29)

        # Add control UI to stage
        self.stage.add_actor(self.vtexture)
        self.stage.add_actor(self.control)

        self._show_controls(not controls_visible)
        self._position_controls()

        self.stage.hide_cursor()
        # TODO: Waiting for G-I bugfix
        # self.control.animatev(Clutter.AnimationMode.EASE_OUT_QUINT, 1000, ["opacity"], [0])

        # Hook up other events
        self.stage.connect("event", self._input_cb)

        # Insert a broadcast logo, if provided
        if logo is not None:
            self.logo = Clutter.Texture().new_from_file(logo)
            self.logo.hide()
            self.logo_width, self.logo_height = self.logo.get_size()
            self.stage.add_actor(self.logo)
        else:
            self.logo = None

        self.vtexture.set_playing(True)
        self.stage.show();


    def _show_controls(self, visible):
        if visible and self.controls_showing:
            """
            if self.controls_timeout == 0:
                self.controls_timeout = # TODO TIMEOUT 
            """
            visible = False
        
        if visible and not self.controls_showing:
            self.controls_showing = True
            self.control_progress = self.vtexture.connect("notify::progress", self._tick)
            self.stage.show_cursor()
            self.control.show()
            # TODO: Waiting for G-I bugfix
            # self.control.animatev(Clutter.EASE_OUT_QUINT, 250, "opacity", 224)
        elif not visible and self.controls_showing:
            self.controls_showing = False
            self.stage.hide_cursor()
            self.control.hide()
            self.vtexture.disconnect(self.control_progress)

            # TODO: Waiting for G-I bugfix
            #self.control.animate(Clutter.EASE_OUT_QUINT, 250, "opacity", 0)


    def _toggle_pause_state(self):
        if self.paused:
            self.vtexture.set_playing(True)
            self.paused = False
            self.control_play.hide()
            self.control_pause.show()
        else:
            self.vtexture.set_playing(False)
            self.paused = True
            self.control_play.show()
            self.control_pause.hide()


    def _reset_animation(self, animation):
        self.vtexture.set_rotation(CLUTTER_Y_AXIS, 0.0, 0, 0, 0)


    def _input_cb(self, stage, event):
        handled = False

        if event.type == Clutter.EventType.MOTION:
            self.show_controls(True)
            handled = True

        elif event.type == Clutter.EventType.BUTTON_PRESS:
            if self.controls_showing:
                actor = self.stage.get_actor_at_pos(CLUTTER_PICK_ALL, event.x, event.y)

                if actor == self.control_pause or actor == self.control_play:
                    self._toggle_pause_state()
                elif actor == self.control_seek1 or actor == self.control_seek2 or actor == self.control_seekbar:
                    x, y = self.control_seekbar.get_transformed_position()
                    
                    dist = event.x - x

                    # dist = CLAMP (dist, 0, self.SEEK_W) #TODO

                    progress = dist / self.SEEK_W

                    self.vtexture.set_progress(progress)
            handled = True

        elif event.type == Clutter.EventType.KEY_PRESS:
            center = Clutter.Vertex( 0 )

            center.x = self.vtexture.get_width() / 2

            symbol = event.get_key_symbol()

            if symbol == Clutter.KEY_d:
                self.stage.remove_actor(self.vtexture)
                self.vtexture = None
                self.stage.remove_actor(self.control)
                self.control = None
            elif symbol == Clutter.KEY_q or symbol == Clutter.KEY_Escape:
                self.destroy()
            elif symbol == Clutter.KEY_e:
                #TODO: Waiting for G-I bugfix
                #animation = self.vtexture.animate(CLUTTER_LINEAR, 500, "rotation-angle-y", 360.0, "fixed::rotation-center-y", center)
                #animation.connect("completed", self._reset_animation)
                handled = True
            else:
                self.toggle_pause_state()
                handled + True

        return handled 
    

    def _size_change(self, texture, base_width, base_height):
        stage_width, stage_height = self.stage.get_size()

        """
        base_width and base_height are the actual dimensions of the buffers before
        taking the pixel aspect ratio into account. We need to get the actual
        size of the texture to display
        """

        texture.set_size(base_width, base_height)
        frame_width, frame_height = texture.get_size()

        """
        Anamorph 16:9 output.
        Normal television was 4:3, Widescreen television is 16:9.
        This means that normal television is 12:9.
        The of pixels we have to stretch a widescreen image is therefore: 16:12

        Given too choices: a fixed anamorph 16:9 output or an output that only
        streches widescreen to increase vertical resolution. We have to provide
        meta information calling the WideScreenSignalling.

        First we implement fixed anamorph 16:9 output, this means we have to
        scale 4:3 output to the proper aspect ratio (horizontally)
        """

        print {'base_width': base_width, 'base_height': base_height}
        print {'stage_width': stage_width, 'stage_height': stage_height}
        print {'frame_width': frame_width, 'frame_height': frame_height}

        if self.anamorph and (3 * stage_width / 4) == stage_height:
            aspectchanger = 16.0/12.0
        else:
            aspectchanger = 1

        new_height = (aspectchanger * frame_height * stage_width) / frame_width
        if new_height <= stage_height:
            # Widescreen
            new_width = stage_width

            new_x = 0;
            new_y = (stage_height - new_height) / 2
        else:
            # 4:3
            new_width  = (frame_width * stage_height) / (frame_height * aspectchanger)
            new_height = stage_height

            new_x = (stage_width - new_width) / 2
            new_y = 0

        texture.set_position(new_x, new_y)
        texture.set_size(new_width, new_height)
 
        if self.logo is not None:
            self.logo.set_position(new_x + new_width - self.logo_width - 10, new_y + 10)
            if self.anamorph:
                # TODO: allow for search for 'in aspect' logo's
                if new_height <= stage_height:
                    self.logo.set_size(self.logo_width, self.logo_height * aspectchanger)
                else:
                    self.logo.set_size(self.logo_width / aspectchanger, self.logo_height)
            self.logo.show()

    def _position_controls(self):
        stage_width, stage_height = self.stage.get_size()
        bg_width, bg_height = self.control.get_size()

        x = int((stage_width - bg_width) / 2)
        y = stage_height - bg_height - 28;

        self.control.set_position(x, y)


    def _on_stage_allocation_changed(self, stage, box, flags):
        self._position_controls()
        self._show_controls(True)


    def _tick(self, media, pspec):
        progress = media.get_progress()
        self.control_seekbar.set_size(progress * self.SEEK_W,
                                      self.SEEK_H)


    def _on_video_texture_eos(self, media):
        if self.playlist.loop and len(self.playlist.entries) == 1:
            media.set_progress(0.0)
            media.set_playing(True)
        else:
            filename = self.playlist.advance()
            if filename is not None:
                media.set_filename(filename)
                media.set_playing(True)
            else:
                exit()
    
    def renderText(self):
        """
        Create a ClutterText with the phrase Hello World
        """
        txtFont = "Mono 10"
        self.clutterText = Clutter.Text.new_full(txtFont, "Hello World", colorWhite)
        Clutter.Container.add_actor(self.mainWindow, self.clutterText)
        self.clutterText.set_position(5,5)
        self.clutterText.show()


    def renderRect(self):
        """
        Create a basic colored rectangle
        """
        self.clutterRectangle = Clutter.Rectangle.new_with_color(colorMuddyBlue)
        self.clutterRectangle.set_size(200,50)
        Clutter.Container.add_actor(self.mainWindow, self.clutterRectangle)
        self.clutterRectangle.show()


    def destroy(self):
        Clutter.main_quit()


################################################################################
# Main
################################################################################
def main():
    if len(sys.argv) < 2:
        print "Usage: %s [OPTIONS] <video file> [logo file]" % (sys.argv[0])
    else:
        if DEBUG:
            Clutter.init(debugArgs)
        else:
            Clutter.init(sys.argv)

        filename = sys.argv[1]
        if len(sys.argv) > 2:
            logo = sys.argv[2]
        else:
            logo = None
        app = VideoApp(filename, logo, False, False, False, True)
        Clutter.main()

if __name__ == "__main__":
    sys.exit(main())
