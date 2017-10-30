Plugins
=======
Almost all custom functionality is added through plugin modules, which
are typically dynamic libraries or scripts.  The ability to capture
and/or output audio/video, make a recording, output to an RTMP stream,
encode in x264 are all examples of things that are accomplished via
plugin modules.

Plugins can implement sources, outputs, encoders, and services.

Plugin Module Headers
---------------------
These are some notable headers commonly used by plugins:

- `libobs/obs-module.h`_ -- The primary header used for creating plugin
  modules.  This file automatically includes the following files: 

  - `libobs/obs.h`_ -- The main libobs header.  This file automatically
    includes the following files:

    - `libobs/obs-source.h`_ -- Used for implementing sources in plugin
      modules

    - `libobs/obs-output.h`_ -- Used for implementing outputs in plugin
      modules

    - `libobs/obs-encoder.h`_ -- Used for implementing encoders in
      plugin modules

    - `libobs/obs-service.h`_ -- Used for implementing services in
      plugin modules

    - `libobs/graphics/graphics.h`_ -- Used for graphics rendering

Common Directory Structure and CMakeLists.txt
---------------------------------------------
This is an example of a common directory structure for a native plugin
module::

  my-plugin/data/locale/en-US.ini
  my-plugin/CMakeLists.txt
  my-plugin/my-plugin.c
  my-plugin/my-source.c
  my-plugin/my-output.c
  my-plugin/my-encoder.c
  my-plugin/my-service.c

This would be an example of a common CMakeLists.txt file associated with
these files::

  # my-plugin/CMakeLists.txt

  project(my-plugin)

  set(my-plugin_SOURCES
        my-plugin.c
        my-source.c
        my-output.c
        my-encoder.c
        my-service.c)

  add_library(my-plugin MODULE
        ${my-plugin_SOURCES})
  target_link_libraries(my-plugin
        libobs)

  install_obs_plugin_with_data(my-plugin data)

Creating Native Plugin Modules
------------------------------
The common way source files are organized is to have one file for plugin
initialization, and then specific files for each individual object
you're implementing.  For example, if you were to create a plugin called
'my-plugin', you'd have something like my-plugin.c where plugin
initialization is done, my-source.c for the definition of a custom
source, my-output.c for the definition of a custom output, etc.

To create a native plugin module, you will need to include the
`libobs/obs-module.h`_ header, use `OBS_DECLARE_MODULE()`_ macro, then
create a definition of the function obs_module_load_.  In your
obs_module_load_ function, you then register any of your custom sources,
outputs, encoders, or services.

The following is an example of my-plugin.c, which would register one
object of each type:

.. code:: cpp

  /* my-plugin.c */
  #include <obs-module.h>

  /* Defines common functions (required) */
  OBS_DECLARE_MODULE()

  /* Implements common ini-based locale (optional) */
  OBS_MODULE_USE_DEFAULT_LOCALE("my-plugin", "en-US")

  extern struct obs_source_info  my_source;  /* Defined in my-source.c  */
  extern struct obs_output_info  my_output;  /* Defined in my-output.c  */
  extern struct obs_encoder_info my_encoder; /* Defined in my-encoder.c */
  extern struct obs_service_info my_service; /* Defined in my-service.c */

  bool obs_module_load(void)
  {
          obs_register_source(&my_source);
          obs_register_output(&my_output);
          obs_register_encoder(&my_encoder);
          obs_register_service(&my_service);
          return true;
  }

.. _plugins_sources:

Sources
-------
Sources are used to render video and/or audio on stream.  Things such as
capturing displays/games/audio, playing a video, showing an image, or
playing audio.  The `libobs/obs-source.h`_ is the dedicated header for
implementing sources.

For example, to implement a source object, you need to define an
obs_source_info_ structure and fill it out with information and
callbacks related to your source:

.. code:: cpp

  /* my-source.c */

  [...]

  struct obs_source_info my_source {
          .id           = "my_source",
          .type         = OBS_SOURCE_TYPE_INPUT,
          .output_flags = OBS_SOURCE_VIDEO,
          .get_name     = my_source_name,
          .create       = my_source_create,
          .destroy      = my_source_destroy,
          .update       = my_source_update,
          .video_render = my_source_render,
          .get_width    = my_source_width,
          .get_height   = my_source_height
  };

Then, in my-plugin.c, you would call obs_register_source_ in
obs_module_load_ to register the source with libobs.

.. code:: cpp

  /* my-plugin.c */

  [...]
  
  extern struct obs_source_info  my_source;  /* Defined in my-source.c  */

  bool obs_module_load(void)
  {
          obs_register_source(&my_source);

          [...]

          return true;
  }

.. _plugins_outputs:

Outputs
-------

.. _plugins_encoders:

Encoders
--------

.. _plugins_services:

Services
--------

.. _obs_source_info: https://github.com/jp9000/obs-studio/blob/2c58185af3c85f4e594a4c067c9dfe5fa4b5b0a9/libobs/obs-source.h#L145-L431
.. _obs_register_source: https://github.com/jp9000/obs-studio/blob/2c58185af3c85f4e594a4c067c9dfe5fa4b5b0a9/libobs/obs-source.h#L433-L443
.. _libobs/obs-module.h: https://github.com/jp9000/obs-studio/blob/master/libobs/obs-module.h
.. _libobs/obs.h: https://github.com/jp9000/obs-studio/blob/master/libobs/obs.h
.. _libobs/obs-source.h: https://github.com/jp9000/obs-studio/blob/master/libobs/obs-source.h
.. _libobs/obs-output.h: https://github.com/jp9000/obs-studio/blob/master/libobs/obs-output.h
.. _libobs/obs-encoder.h: https://github.com/jp9000/obs-studio/blob/master/libobs/obs-encoder.h
.. _libobs/obs-service.h: https://github.com/jp9000/obs-studio/blob/master/libobs/obs-service.h
.. _libobs/graphics/graphics.h: https://github.com/jp9000/obs-studio/blob/master/libobs/graphics/graphics.h
.. _OBS_DECLARE_MODULE(): https://github.com/jp9000/obs-studio/blob/2c58185af3c85f4e594a4c067c9dfe5fa4b5b0a9/libobs/obs-module.h#L75-L85
.. _obs_module_load: https://github.com/jp9000/obs-studio/blob/2c58185af3c85f4e594a4c067c9dfe5fa4b5b0a9/libobs/obs-module.h#L87-L95
