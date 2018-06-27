---
layout: post
title: "Surface SurfaceView TextureView SurfaceTexture GLSurfaceView的关系"
category: Android

---



# Surface

Handle onto a raw buffer that is being managed by the screen compositor.

A Surface is generally created by or from a consumer of image buffers (such as a SurfaceTexture, MediaRecorder, or Allocation), and is handed to some kind of producer (such as OpenGL, MediaPlayer, or CameraDevice) to draw into.

Note: A Surface acts like a weak reference to the consumer it is associated with. By itself it will not keep its parent consumer from being reclaimed.


# SurfaceView

Provides a dedicated drawing surface embedded inside of a view hierarchy. You can control the format of this surface and, if you like, its size; the SurfaceView takes care of placing the surface at the correct location on the screen

The surface is Z ordered so that it is behind the window holding its SurfaceView; the SurfaceView punches a hole in its window to allow its surface to be displayed. The view hierarchy will take care of correctly compositing with the Surface any siblings of the SurfaceView that would normally appear on top of it. This can be used to place overlays such as buttons on top of the Surface, though note however that it can have an impact on performance since a full alpha-blended composite will be performed each time the Surface changes.


# SurfaceTexture

>Captures frames from an image stream as an OpenGL ES texture.

The image stream may come from either camera preview or video decode. A Surface created from a SurfaceTexture can be used as an output destination for the android.hardware.camera2, MediaCodec, MediaPlayer, and Allocation APIs. When updateTexImage() is called, the contents of the texture object specified when the SurfaceTexture was created are updated to contain the most recent image from the image stream. This may cause some frames of the stream to be skipped.

A SurfaceTexture may also be used in place of a SurfaceHolder when specifying the output destination of the older Camera API. Doing so will cause all the frames from the image stream to be sent to the SurfaceTexture object rather than to the device's display.

When sampling from the texture one should first transform the texture coordinates using the matrix queried via getTransformMatrix(float[]). The transform matrix may change each time updateTexImage() is called, so it should be re-queried each time the texture image is updated. This matrix transforms traditional 2D OpenGL ES texture coordinate column vectors of the form (s, t, 0, 1) where s and t are on the inclusive interval [0, 1] to the proper sampling location in the streamed texture. This transform compensates for any properties of the image stream source that cause it to appear different from a traditional OpenGL ES texture. For example, sampling from the bottom left corner of the image can be accomplished by transforming the column vector (0, 0, 0, 1) using the queried matrix, while sampling from the top right corner of the image can be done by transforming (1, 1, 0, 1).

The texture object uses the GL_TEXTURE_EXTERNAL_OES texture target, which is defined by the GL_OES_EGL_image_external OpenGL ES extension. This limits how the texture may be used. Each time the texture is bound it must be bound to the GL_TEXTURE_EXTERNAL_OES target rather than the GL_TEXTURE_2D target. Additionally, any OpenGL ES 2.0 shader that samples from the texture must declare its use of this extension using, for example, an "#extension GL_OES_EGL_image_external : require" directive. Such shaders must also access the texture using the samplerExternalOES GLSL sampler type.

SurfaceTexture objects may be created on any thread. updateTexImage() may only be called on the thread with the OpenGL ES context that contains the texture object. The frame-available callback is called on an arbitrary thread, so unless special care is taken updateTexImage() should not be called directly from the callback.
