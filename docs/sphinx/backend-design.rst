OBS Studio Backend Design Introduction
======================================
The OBS Studio backend is powered by the library libobs.  Libobs
provides the main pipeline, the video/audio subsystems, and the general
framework for all plugins.

Libobs Plugin Objects
---------------------
Libobs is designed to be modular, where adding modules will add custom
functionality.  There are four libobs objects that you can make plugins
for:

- Sources -- Sources are used to render video and/or audio on stream.
  Things such as capturing displays/games/audio, playing a movie,
  showing an image, or playing audio.

- Outputs -- Outputs allow the ability to output the currently rendering
  audio/video.  Streaming and recording are two common examples of
  outputs, but not the only types of outputs.  Outputs can receive the
  raw data or receive encoded data.

- Encoders -- Encoders are OBS-specific implementations of video/audio
  encoders, which are used with outputs that use encoders.

- Services -- Services are custom implementations of streaming services,
  which are used with outputs that stream.  For example, you could have
  a custom implementation for streaming to Twitch, and another for
  YouTube to allow the ability to log in and use their APIs to do things
  such as get the RTMP servers or control the channel.
  
*(Author's note: the service API is incomplete as of this writing, and
service-specific implementations have not yet been fully implemented)*

Libobs Threads
--------------
There are three primary threads spawned by libobs on initialization:

- The obs_graphics_thread (`ref
  <https://github.com/jp9000/obs-studio/blob/20.1.1/libobs/obs-video.c#L588-L651>`_)
  function used exclusively for rendering in `libobs/obs-video.c
  <https://github.com/jp9000/obs-studio/blob/master/libobs/obs-video.c>`_
- The video_thread function used exclusively for video encoding/output
  in `libobs/media-io/video-io.c
  <https://github.com/jp9000/obs-studio/blob/master/libobs/media-io/video-io.c>`_
- The audio_thread function used for all audio
  processing/encoding/output in `libobs/media-io/audio-io.c
  <https://github.com/jp9000/obs-studio/blob/master/libobs/media-io/audio-io.c>`_

*(Author note: obs_graphics_thread was originally named
obs_video_thread; it was renamed as of ths writing to prevent confusion
with video_thread)*

Output Channels
---------------
Rendering video or audio starts from output channels.  You assign a
source to an output channel via this function (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs.h>`_)::

  void obs_set_output_source(uint32_t channel, obs_source_t *source);

*channel* can be any number from *0..(MAX_CHANNELS-1)* (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-defs.h>`_).
You may initially think that this is how you display multiple sources at
once; however, sources are hierarchical.  Sources such as scenes or
transitions can have multiple sub-sources, and those sub-sources in turn
can have sub-sources and so on.  Typically, you would use scenes to draw
multiple sources as a group with specific transforms for each source, as
a scene is just another type of source.  The "channel" design allows for
highly complex video presentation setups.  The OBS Studio front-end has
yet to even fully utilize this back-end design for its rendering, and
currently only uses one output channel to render one scene at a time.
It does however utilize additional channels for things such as global
audio sources which are set in audio settings.

*(Author note: "Output channels" are not to be confused with output
objects.  Output channels are used to set the sources you want to
output, and output objects are used for actually
streaming/recording/etc)*

General Video Pipeline Overview
-------------------------------
The video graphics pipeline is run from two threads: a dedicated
graphics thread that renders preview displays as well as the final mix
(the obs_graphics_thread function in `libobs/obs-video.c
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-video.c>`_),
and a dedicated thread specific to video encoding/output (the
video_thread function in `libobs/media-io/video-io.c
<https://github.com/jp9000/obs-studio/blob/master/libobs/media-io/video-io.c>`_).

Sources assigned to outputs channels will be drawn from channels
*0..(MAX_CHANNELS-1)*.  They are drawn on to the final texture which
will be used for output (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-video.c>`_).
Once all sources are drawn, the final texture is converted to whatever
format that libobs is set to (typically a YUV format).  After being
converted to the back-end video format, it's then sent along with its
timestamp to the current video handler, obs_core_video::video (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-internal.h>`_).

It then puts that raw frame in a queue of *MAX_CACHE_SIZE* in the video
output (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/media-io/video-io.c>`_).
A semaphore is posted, then the video-io thread will process frames as
it's able.  If the video frame cache is full, it will duplicate the last
frame in the queue in an attempt to reduce video encoding complexity
(and thus CPU usage).  This is why you may see frame skipping when the
encoder can't keep up.  Frames are sent to any raw outputs or video
encoders that are currently active.

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
Similarly, the audio pipeline is run from a dedicated audio thread in
the audio handler (the audio_thread function in
`libobs/media-io/audio-io.c
<https://github.com/jp9000/obs-studio/blob/master/libobs/media-io/audio-io.c>`_);
assuming that *AUDIO_OUTPUT_FRAMES* (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/media-io/audio-io.h>`_)
is set to 1024, the audio thread "ticks" (processes audio data) once
every 1024 audio samples (around every 21 millisecond intervals at
48khz), and calls the audio_callback function in libobs/obs-audio.c
where most of the audio processing is accomplished (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-audio.c>`_).

A source with audio will output its audio via the
obs_source_output_audio function (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs.h>`_), and
that audio data will be appended or inserted in to the circular buffer
obs_source::audio_input_buf (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-internal.h>`_).
If the sample rate or channel count does not match what the back-end is
set to, the audio is automatically remixed/resampled via swresample.
Before insertion, audio data is also run through any audio filters
attached to the source.

Each audio tick, the audio thread takes a reference snapshot of the
audio source tree (stores references of all sources that output/process
audio).  On each audio leaf (audio source), it takes the closest audio
(relative to the current audio thread timestamp) stored in the circular
buffer obs_source::audio_input_buf, and puts it in
obs_source::audio_output_buf.

Then, the audio samples stored in obs_source::audio_output_buf of the
leaves get sent through their parents in the source tree snapshot for
mixing or processing at each source node in the hierarchy.  Sources with
multiple children such as scenes or transitions will mix/process their
children's audio themselves via the obs_source_info::audio_render
callback (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-source.h>`_).
This allows, for example, transitions to fade in the audio of one source
and fade in the audio of a new source when they're transitioning between
two sources.  The mix or processed audio data is then stored in
obs_source::audio_output_buf of that node similarly.

Finally, when the audio has reached the base of the snapshot tree, the
audio of all the sources in each output channel are mixed together for a
final mix.  That final mix is then sent to any raw outputs or audio
encoders that are currently active.

If it's sent to an audio encoder object (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-encoder.c>`_),
it encodes the audio data and sends the encoded packet off to the
outputs that encoder is connected to (which can be multiple).  If the
output takes both encoded video/audio, it puts the packets in an
interleave queue to ensure encoded packets are sent in monotonic
timestamp order.

The encoded packet or raw frame is then sent to the output (`ref
<https://github.com/jp9000/obs-studio/blob/master/libobs/obs-output.c>`_).
