OBS Studio Backend Design Introduction
======================================
The OBS Studio backend is powered by the library libobs.  Libobs
provides the main pipeline, graphics and audio subsystems, and the
general framework for all plugins.

Libobs Plugins
--------------
Libobs is designed to be modular, where adding modules will add custom
functionality.  There are four libobs objects that you can make plugins
for:

- Inputs -- Inputs are used to render video and/or audio on stream.
  Things such as capturing displays/games/audio, playing a movie, or
  showing an image.

- Outputs -- Outputs allow the ability to output the currently rendering
  audio/video.  Streaming and recording are two common examples of
  outputs, but not the only types of outputs.  Outputs can receive the
  raw data or receive encoded data.

- Encoders -- Encoders are OBS-specific implementations of video/audio
  encoders, which are used with outputs that use encoders.

- Services -- Services are custom implementations of streaming services.
  For example, you could have a custom implementation for streaming to
  Twitch, and another for YouTube to allow the ability to log in and use
  their APIs to do things such as get the RTMP servers or control the
  channel.

General Video Pipeline Overview
-------------------------------
The video graphics pipeline is run from a dedicated graphics thread, and
starts from the output sources.  You create a source, assign it to an
output channel via this function (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs.h>`_)::

  void obs_set_output_source(uint32_t channel, obs_source_t *source);

*channel* can be any number from *0..(MAX_CHANNELS-1)* (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-defs.h>`_).
You may initially think that this is how you display multiple sources at
once; however, sources are hierarchical.  Sources such as scenes or
transitions can have multiple sub-sources, and they in turn can have
sub-sources.  Typically, you would use scenes to draw multiple sources
as a group with specific transforms for each source, as a scene is just
another type of source.

Sources assigned to these outputs channels will be drawn from channels
*0..(MAX_CHANNELS-1)*.  They are drawn on to the final texture which
will be used for output (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-video.c>`_).
Once all sources are drawn, the final texture is converted to whatever
format that libobs is set to (typically a YUV format).  After being
converted to the back-end format, it's then sent along with its
timestamp to the current video handler, obs_core_video::video (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-internal.h>`_).

It then put that raw frame in a queue of *MAX_CACHE_SIZE* in the video
output (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/media-io/video-io.c>`_).
A semaphore is posted, then the video-io thread will process frames as
it's able.  If the video frame cache is full, it will duplicate the last
frame in the queue in an attempt to reduce video encoding complexity
(and thus CPU usage).  This is why you may see frame skipping when the
encoder can't keep up.  Frames are sent to the outputs or video encoders
that are currently active.

If it's sent to a video encoder object (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-encoder.c>`_),
it encodes the frame and sends the encoded packet off to the outputs
that encoder is connected to (which can be multiple).  If the output
takes both encoded video/audio, it puts the packets in an interleave
queue to ensure encoded packets are sent in monotonic timestamp order.

The encoded packet or raw frame is then sent to the output (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-output.c>`_).

General Audio Pipeline Overview
-------------------------------
Similarly, the audio pipeline is run from a dedicated audio thread (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-audio.c>`_).
The audio thread processes audio data ("ticks") every 1024 audio samples
(approximately 21ms at 48khz).

A source with audio outputs its audio via the obs_source_output_audio
function (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs.h>`_), and
appends or inserts that audio data in to the circular buffer
obs_source::audio_input_buf (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-internal.h>`_).

Each audio tick, it takes a reference snapshot of the audio source tree
(stores references of all sources that output/process audio).  On each
audio leaf (audio source), it takes the closest audio (time-wise) stored
in the circular buffer obs_source::audio_input_buf, and puts it in
obs_source::audio_output_buf.

Then, it goes through the source tree snapshot, and then sends the audio
down the tree for mixing.  Sources with multiple children such as scenes
or transition will mix their own audio themselves from their children.
Transitions will fade in/out audio from their transitioning scenes.

Finally, when the sources have reached the output channels, all output
channels are mixed together for a final mix, and then that data is sent
to the current audio handler, obs_core_audio::audio (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-internal.h>`_).
The audio data is then resampled in the audio handler (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/media-io/audio-io.c>`_)
if necessary, and sent to audio encoders or outputs.

If it's sent to an audio encoder object (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-encoder.c>`_),
it encodes the audio data and sends the encoded packet off to the
outputs that encoder is connected to (which can be multiple).  If the
output takes both encoded video/audio, it puts the packets in an
interleave queue to ensure encoded packets are sent in monotonic
timestamp order.

The encoded packet or raw frame is then sent to the output (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-output.c>`_).
