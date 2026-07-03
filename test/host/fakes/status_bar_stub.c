/* Host-test stub: navigator_render() overlays the status bar; the tests assert screen
   content only, so drawing nothing keeps their pixel assertions unchanged. */
void status_bar_draw(void) {}
