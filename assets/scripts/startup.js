// Default offline runtime script. Console snippets share `this` with callbacks.
// Keep this lightweight; performance checks report callback time separately.
this.frames = 0;
ak.onFrame(function () { this.frames++; });
