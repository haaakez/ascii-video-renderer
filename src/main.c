#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>
#include <epoxy/gl.h>
#include <glib/gstdio.h>
#include <cairo.h>
#include <pango/pangocairo.h>

#include <math.h>
#include <string.h>

typedef struct {
    guint8 *pixels;
    gsize size;
    gint width;
    gint height;
    gint stride;
} VideoFrame;

typedef enum {
    ASCII_COLOR_GRAYSCALE = 0,
    ASCII_COLOR_SOURCE = 1,
    ASCII_COLOR_PALETTE = 2
} AsciiColorMode;

typedef struct {
    gint cell_width;
    gint cell_height;
    gint ramp_levels;
    gfloat glyph_aspect;
    AsciiColorMode color_mode;
    gfloat brightness;
    gfloat contrast;
    gfloat gamma;
    gfloat saturation;
    gfloat edge;
    gfloat threshold;
    gboolean threshold_enabled;
    gboolean invert;
    gfloat foreground[3];
    gfloat background[3];
    gfloat palette[3][3];
    gint ramp_glyphs[10];
    gchar *ramp;
    gchar *font_family;
} RenderSettings;

typedef struct {
    const gchar *ramp;
    AsciiColorMode color_mode;
    const gchar *foreground;
    const gchar *background;
    const gchar *palette;
    gdouble brightness;
    gdouble contrast;
    gdouble gamma;
    gdouble saturation;
    gdouble edge;
    gdouble threshold;
    gboolean threshold_enabled;
    gboolean invert;
} PresetValues;

typedef struct {
    guint8 *source_pixels;
    guint8 *ascii_pixels;
    gsize size;
    gint width;
    gint height;
    gint source_width;
    gint source_height;
} RenderResult;

typedef struct _AppState AppState;

struct _AppState {
    GtkApplication *application;
    GtkApplicationWindow *window;
    GtkGLArea *gl_area;
    GtkGLArea *source_gl_area;
    GtkPicture *ascii_picture;
    GtkPicture *source_picture;
    GtkWidget *empty_state;
    GtkWidget *source_empty_state;
    GtkToggleButton *play_button;
    GtkCheckButton *loop_button;
    GtkCheckButton *invert_button;
    GtkScale *progress_scale;
    GtkDropDown *speed_dropdown;
    GtkDropDown *preset_dropdown;
    GtkDropDown *font_dropdown;
    GtkDropDown *color_mode_dropdown;
    GtkDropDown *format_dropdown;
    GtkDropDown *quality_dropdown;
    GtkDropDown *resolution_dropdown;
    GtkWidget *custom_resolution_grid;
    GtkSpinButton *custom_width_spin;
    GtkSpinButton *custom_height_spin;
    gboolean custom_resolution_user_set;
    gboolean syncing_custom_resolution;
    GtkButton *export_button;
    GtkSpinButton *ascii_size_spin;
    GtkEntry *ramp_entry;
    GtkEntry *foreground_entry;
    GtkEntry *background_entry;
    GtkEntry *palette_entry;
    GtkScale *glyph_aspect_scale;
    GtkScale *brightness_scale;
    GtkScale *contrast_scale;
    GtkScale *gamma_scale;
    GtkScale *saturation_scale;
    GtkScale *edge_scale;
    GtkScale *threshold_scale;
    GtkDropDown *threshold_dropdown;
    GtkLabel *position_label;
    GtkLabel *duration_label;
    GtkLabel *status_label;
    GtkLabel *title_label;
    GtkLabel *source_meta_label;

    GstElement *pipeline;
    guint bus_watch_id;
    guint position_timer_id;
    gint64 duration;
    gboolean updating_position;
    gboolean playing;

    GMutex frame_lock;
    VideoFrame *latest_frame;
    VideoFrame *latest_source_frame;

    GThread *render_thread;
    GMutex render_lock;
    GCond render_cond;
    gboolean render_stop;
    gboolean render_pending;
    gboolean worker_settings_ready;
    RenderSettings worker_settings;
    RenderResult *render_result;
    guint render_present_source_id;
    gint preview_width;
    gint preview_height;

    gint cell_width;
    gint cell_height;
    gfloat glyph_aspect;
    gint ramp_levels;
    AsciiColorMode color_mode;
    gfloat brightness;
    gfloat contrast;
    gfloat gamma;
    gfloat saturation;
    gfloat edge;
    gfloat threshold;
    gboolean threshold_enabled;
    gboolean invert;
    gboolean loop;
    gdouble playback_rate;
    gchar *video_path;
    gboolean cpu_fallback;

    GstElement *export_decode_pipeline;
    GstElement *export_output_pipeline;
    GstElement *export_appsrc;
    guint export_decode_watch_id;
    guint export_output_watch_id;
    gchar *export_path;
    gchar *export_temp_dir;
    gchar *export_temp_pattern;
    gint export_format;
    gint export_width;
    gint export_height;
    guint64 export_submitted_frame;
    guint64 export_next_write_frame;
    gint64 export_frame_duration;
    gboolean exporting;
    RenderSettings export_settings;
    GThreadPool *export_pool;
    GThread *export_writer_thread;
    GMutex export_lock;
    GCond export_cond;
    GCond export_space_cond;
    GHashTable *export_results;
    guint export_workers;
    guint export_pending;
    guint export_max_pending;
    gboolean export_decode_eos;
    gboolean export_stop;
    guint export_failure_source_id;

    gboolean gl_ready;
    GLuint shader_program;
    GLuint vertex_array;
    GLuint video_texture;
    GLint video_uniform;
    GLint video_size_uniform;
    GLint viewport_size_uniform;
    GLint cell_size_uniform;
    GLint color_uniform;
    GLint tint_uniform;
    GLint contrast_uniform;
    GLint gamma_uniform;
    GLint brightness_uniform;
    GLint saturation_uniform;
    GLint edge_uniform;
    GLint invert_uniform;
    GLint threshold_enabled_uniform;
    GLint threshold_uniform;
    GLint ramp_levels_uniform;
    GLint foreground_uniform;
    GLint background_uniform;
    GLint palette0_uniform;
    GLint palette1_uniform;
    GLint palette2_uniform;
    GLint color_mode_uniform;
    gint texture_width;
    gint texture_height;

    GLuint source_shader_program;
    GLuint source_vertex_array;
    GLuint source_video_texture;
    GLint source_video_uniform;
    GLint source_video_size_uniform;
    GLint source_viewport_size_uniform;
    gint source_texture_width;
    gint source_texture_height;
};

typedef struct {
    AppState *app;
    guint64 frame_number;
    VideoFrame frame;
    RenderSettings settings;
} ExportFrameTask;

typedef struct {
    guint64 frame_number;
    guint8 *pixels;
    gsize size;
    gint width;
    gint height;
} ExportFrameResult;

static void build_window(AppState *app);
static void stop_export(AppState *app, gboolean success);

static const char *vertex_shader_source =
    "#version 330 core\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    const vec2 positions[3] = vec2[3](\n"
    "        vec2(-1.0, -1.0),\n"
    "        vec2( 3.0, -1.0),\n"
    "        vec2(-1.0,  3.0)\n"
    "    );\n"
    "    vec2 position = positions[gl_VertexID];\n"
    "    v_uv = position * 0.5 + 0.5;\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "}\n";

/*
 * The glyphs are deliberately kept in the shader. This avoids a font
 * dependency and means glyph selection and rasterization are both GPU work.
 * Each row is a five-bit bitmap for the brightness ramp "@%#*+=-:. ".
 */
static const char *fragment_shader_source =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    "out vec4 frag_color;\n"
    "uniform sampler2D u_video;\n"
    "uniform vec2 u_video_size;\n"
    "uniform vec2 u_viewport_size;\n"
    "uniform vec2 u_cell_size;\n"
    "uniform int u_color_mode;\n"
    "uniform vec3 u_tint;\n"
    "uniform vec3 u_foreground;\n"
    "uniform vec3 u_background;\n"
    "uniform vec3 u_palette0;\n"
    "uniform vec3 u_palette1;\n"
    "uniform vec3 u_palette2;\n"
    "uniform float u_brightness;\n"
    "uniform float u_contrast;\n"
    "uniform float u_gamma;\n"
    "uniform float u_saturation;\n"
    "uniform float u_edge;\n"
    "uniform bool u_invert;\n"
    "uniform bool u_threshold_enabled;\n"
    "uniform float u_threshold;\n"
    "uniform int u_ramp_levels;\n"
    "\n"
    "int glyph_row(int glyph, int row) {\n"
    "    if (glyph == 0) return 0;\n"
    "    if (glyph == 1) return row == 6 ? 4 : 0;\n"
    "    if (glyph == 2) return (row == 2 || row == 5) ? 4 : 0;\n"
    "    if (glyph == 3) return row == 3 ? 31 : 0;\n"
    "    if (glyph == 4) return (row == 2 || row == 4) ? 31 : 0;\n"
    "    if (glyph == 5) return (row == 3) ? 31 : ((row >= 1 && row <= 5) ? 4 : 0);\n"
    "    if (glyph == 6) {\n"
    "        if (row == 1 || row == 5) return 4;\n"
    "        if (row == 2 || row == 4) return 21;\n"
    "        return row == 3 ? 14 : 0;\n"
    "    }\n"
    "    if (glyph == 7) return (row == 1 || row == 3 || row == 5) ? 21 : ((row == 2 || row == 4) ? 31 : 0);\n"
    "    if (glyph == 8) {\n"
    "        if (row == 0) return 25;\n"
    "        if (row == 1) return 18;\n"
    "        if (row == 2) return 4;\n"
    "        if (row == 3) return 8;\n"
    "        if (row == 4) return 17;\n"
    "        if (row == 5) return 19;\n"
    "        return 0;\n"
    "    }\n"
    "    if (row == 0 || row == 6) return 14;\n"
    "    if (row == 1 || row == 5) return 17;\n"
    "    if (row == 2 || row == 4) return 21;\n"
    "    return 27;\n"
    "}\n"
    "\n"
    "vec2 fitted_uv(vec2 uv) {\n"
    "    float video_aspect = u_video_size.x / max(u_video_size.y, 1.0);\n"
    "    float viewport_aspect = u_viewport_size.x / max(u_viewport_size.y, 1.0);\n"
    "    if (viewport_aspect > video_aspect) {\n"
    "        float used_width = video_aspect / viewport_aspect;\n"
    "        uv.x = (uv.x - (1.0 - used_width) * 0.5) / used_width;\n"
    "    } else {\n"
    "        float used_height = viewport_aspect / video_aspect;\n"
    "        uv.y = (uv.y - (1.0 - used_height) * 0.5) / used_height;\n"
    "    }\n"
    "    return uv;\n"
    "}\n"
    "\n"
    "vec3 sample_video(vec2 uv) {\n"
    "    vec2 texel = 1.0 / max(u_video_size, vec2(1.0));\n"
    "    vec3 color = texture(u_video, vec2(uv.x, 1.0 - uv.y)).rgb;\n"
    "    color += texture(u_video, vec2(uv.x + texel.x * 0.35, 1.0 - uv.y)).rgb;\n"
    "    color += texture(u_video, vec2(uv.x - texel.x * 0.35, 1.0 - uv.y)).rgb;\n"
    "    color += texture(u_video, vec2(uv.x, 1.0 - uv.y + texel.y * 0.35)).rgb;\n"
    "    color += texture(u_video, vec2(uv.x, 1.0 - uv.y - texel.y * 0.35)).rgb;\n"
    "    return color * 0.2;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 cell = floor(gl_FragCoord.xy / u_cell_size);\n"
    "    vec2 cell_uv = (cell + vec2(0.5)) * u_cell_size / u_viewport_size;\n"
    "    bool outside = cell_uv.x < 0.0 || cell_uv.x > 1.0 || cell_uv.y < 0.0 || cell_uv.y > 1.0;\n"
    "    vec2 source_uv = fitted_uv(cell_uv);\n"
    "    bool outside_video = source_uv.x < 0.0 || source_uv.x > 1.0 || source_uv.y < 0.0 || source_uv.y > 1.0;\n"
    "    if (outside || outside_video) {\n"
    "        frag_color = vec4(0.012, 0.016, 0.022, 1.0);\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    vec3 source = sample_video(source_uv);\n"
    "    float luminance = dot(source, vec3(0.2126, 0.7152, 0.0722));\n"
    "    source += (source - vec3(luminance)) * u_edge;\n"
    "    source = clamp(source, 0.0, 1.0);\n"
    "    source = mix(vec3(luminance), source, u_saturation);\n"
    "    luminance = dot(source, vec3(0.2126, 0.7152, 0.0722));\n"
    "    luminance = clamp((luminance - 0.5) * u_contrast + 0.5 + u_brightness, 0.0, 1.0);\n"
    "    luminance = pow(luminance, u_gamma);\n"
    "    if (u_threshold_enabled) luminance = step(u_threshold, luminance);\n"
    "    if (u_invert) luminance = 1.0 - luminance;\n"
    "    int glyph = int(floor((1.0 - luminance) * float(max(u_ramp_levels - 1, 1))));\n"
    "\n"
    "    vec2 local = fract(gl_FragCoord.xy / u_cell_size);\n"
    "    int glyph_x = int(clamp((local.x - 0.08) / 0.84 * 5.0, 0.0, 4.0));\n"
    "    int glyph_y = int(clamp((1.0 - local.y - 0.10) / 0.80 * 7.0, 0.0, 6.0));\n"
    "    int row = glyph_row(glyph, glyph_y);\n"
    "    float mask = float((row >> (4 - glyph_x)) & 1);\n"
    "\n"
    "    vec3 ink = u_foreground;\n"
    "    if (u_color_mode == 1) ink = source;\n"
    "    if (u_color_mode == 2) {\n"
    "        ink = luminance < 0.34 ? u_palette0 : (luminance < 0.67 ? u_palette1 : u_palette2);\n"
    "    }\n"
    "    ink *= 0.72 + luminance * 0.38;\n"
    "    vec3 background = u_background;\n"
    "    frag_color = vec4(mix(background, ink, mask), 1.0);\n"
    "}\n";

static const char *source_fragment_shader_source =
    "#version 330 core\n"
    "out vec4 frag_color;\n"
    "uniform sampler2D u_video;\n"
    "uniform vec2 u_video_size;\n"
    "uniform vec2 u_viewport_size;\n"
    "void main() {\n"
    "    vec2 uv = gl_FragCoord.xy / u_viewport_size;\n"
    "    float video_aspect = u_video_size.x / max(u_video_size.y, 1.0);\n"
    "    float viewport_aspect = u_viewport_size.x / max(u_viewport_size.y, 1.0);\n"
    "    if (viewport_aspect > video_aspect) {\n"
    "        float used_width = video_aspect / viewport_aspect;\n"
    "        uv.x = (uv.x - (1.0 - used_width) * 0.5) / used_width;\n"
    "    } else {\n"
    "        float used_height = viewport_aspect / video_aspect;\n"
    "        uv.y = (uv.y - (1.0 - used_height) * 0.5) / used_height;\n"
    "    }\n"
    "    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {\n"
    "        frag_color = vec4(0.012, 0.016, 0.022, 1.0);\n"
    "        return;\n"
    "    }\n"
    "    frag_color = texture(u_video, vec2(uv.x, 1.0 - uv.y));\n"
    "}\n";

static void set_status(AppState *app, const gchar *text)
{
    if (app->status_label != NULL) {
        gtk_label_set_text(app->status_label, text);
    }
}

static void free_video_frame(VideoFrame *frame)
{
    if (frame == NULL) {
        return;
    }

    g_free(frame->pixels);
    g_free(frame);
}

static gboolean parse_hex_color(const gchar *text, gfloat color[3])
{
    gchar *end = NULL;
    guint64 value;
    const gchar *hex = text;

    if (hex == NULL) {
        return FALSE;
    }
    if (hex[0] == '#') {
        hex++;
    }
    if (strlen(hex) != 6) {
        return FALSE;
    }

    value = g_ascii_strtoull(hex, &end, 16);
    if (end == NULL || *end != '\0') {
        return FALSE;
    }

    color[0] = (gfloat)((value >> 16) & 0xff) / 255.0f;
    color[1] = (gfloat)((value >> 8) & 0xff) / 255.0f;
    color[2] = (gfloat)(value & 0xff) / 255.0f;
    return TRUE;
}

static void parse_palette(const gchar *text, gfloat palette[3][3])
{
    gchar **parts;
    gint i;

    palette[0][0] = 0.02f;
    palette[0][1] = 0.02f;
    palette[0][2] = 0.02f;
    palette[1][0] = 0.55f;
    palette[1][1] = 0.55f;
    palette[1][2] = 0.55f;
    palette[2][0] = 0.95f;
    palette[2][1] = 0.95f;
    palette[2][2] = 0.95f;

    if (text == NULL) {
        return;
    }

    parts = g_strsplit(text, ",", 3);
    for (i = 0; i < 3 && parts[i] != NULL; i++) {
        g_strstrip(parts[i]);
        parse_hex_color(parts[i], palette[i]);
    }
    g_strfreev(parts);
}

static void clear_render_settings(RenderSettings *settings)
{
    g_clear_pointer(&settings->ramp, g_free);
    g_clear_pointer(&settings->font_family, g_free);
}

static gint ramp_glyph_for_character(gunichar character)
{
    if (g_unichar_isspace(character)) {
        return 0;
    }
    switch (character) {
    case '.': return 1;
    case ':': return 2;
    case ';': return 3;
    case '-': return 4;
    case '=': return 5;
    case '+': return 6;
    case '*': return 7;
    case '#': return 8;
    case '%':
    case '@': return 9;
    case 'i':
    case 'I':
    case '!':
    case '|': return 4;
    case 'r':
    case 's':
    case 'v': return 5;
    case 'X':
    case 'x': return 6;
    case 'A':
    case 'B':
    case '&': return 8;
    case 'M':
    case 'W': return 9;
    default:
        if (g_unichar_isalpha(character) || g_unichar_isdigit(character)) {
            return 6;
        }
        return 5;
    }
}

static void build_ramp_glyphs(RenderSettings *settings)
{
    const gchar *cursor = settings->ramp;
    gint count = 0;

    while (cursor != NULL && *cursor != '\0' && count < 10) {
        gunichar character = g_utf8_get_char(cursor);
        settings->ramp_glyphs[count++] = ramp_glyph_for_character(character);
        cursor = g_utf8_next_char(cursor);
    }
    while (count < 10) {
        settings->ramp_glyphs[count] = count == 0 ? 0 : count;
        count++;
    }
}

static void copy_render_settings(RenderSettings *destination,
                                 const RenderSettings *source)
{
    gchar *ramp = g_strdup(source->ramp);
    gchar *font_family = g_strdup(source->font_family);

    clear_render_settings(destination);
    *destination = *source;
    destination->ramp = ramp;
    destination->font_family = font_family;
}

static void read_render_settings(AppState *app, RenderSettings *settings)
{
    gint ascii_size;
    GObject *font_item;

    memset(settings, 0, sizeof(*settings));
    if (app->ascii_size_spin != NULL) {
        ascii_size = MAX(4, (gint)gtk_spin_button_get_value(app->ascii_size_spin));
        settings->cell_width = ascii_size;
        settings->cell_height = ascii_size;
    } else {
        settings->cell_width = MAX(1, app->cell_width);
        settings->cell_height = MAX(1, app->cell_height);
    }
    settings->glyph_aspect = app->glyph_aspect_scale != NULL ?
                             (gfloat)gtk_range_get_value(GTK_RANGE(app->glyph_aspect_scale)) :
                             app->glyph_aspect;
    settings->color_mode = app->color_mode_dropdown != NULL ?
                           (AsciiColorMode)MIN(gtk_drop_down_get_selected(app->color_mode_dropdown),
                                               (guint)ASCII_COLOR_PALETTE) :
                           app->color_mode;
    settings->brightness = app->brightness_scale != NULL ?
                           (gfloat)gtk_range_get_value(GTK_RANGE(app->brightness_scale)) :
                           app->brightness;
    settings->contrast = app->contrast_scale != NULL ?
                         (gfloat)gtk_range_get_value(GTK_RANGE(app->contrast_scale)) :
                         app->contrast;
    settings->gamma = app->gamma_scale != NULL ?
                      (gfloat)gtk_range_get_value(GTK_RANGE(app->gamma_scale)) :
                      app->gamma;
    settings->saturation = app->saturation_scale != NULL ?
                           (gfloat)gtk_range_get_value(GTK_RANGE(app->saturation_scale)) :
                           app->saturation;
    settings->edge = app->edge_scale != NULL ?
                     (gfloat)gtk_range_get_value(GTK_RANGE(app->edge_scale)) :
                     app->edge;
    settings->threshold = app->threshold_scale != NULL ?
                          (gfloat)gtk_range_get_value(GTK_RANGE(app->threshold_scale)) :
                          app->threshold;
    settings->threshold_enabled = app->threshold_dropdown != NULL ?
                                  gtk_drop_down_get_selected(app->threshold_dropdown) != 0 :
                                  app->threshold_enabled;
    settings->invert = app->invert_button != NULL ?
                       gtk_check_button_get_active(app->invert_button) : app->invert;
    settings->foreground[0] = 0.96f;
    settings->foreground[1] = 0.96f;
    settings->foreground[2] = 0.96f;
    settings->background[0] = 0.012f;
    settings->background[1] = 0.016f;
    settings->background[2] = 0.022f;
    parse_hex_color(app->foreground_entry != NULL ?
                        gtk_editable_get_text(GTK_EDITABLE(app->foreground_entry)) : "#f5f5f0",
                    settings->foreground);
    parse_hex_color(app->background_entry != NULL ?
                        gtk_editable_get_text(GTK_EDITABLE(app->background_entry)) : "#11110f",
                    settings->background);
    parse_palette(app->palette_entry != NULL ?
                      gtk_editable_get_text(GTK_EDITABLE(app->palette_entry)) : NULL,
                  settings->palette);
    settings->ramp = g_strdup(app->ramp_entry != NULL ?
                              gtk_editable_get_text(GTK_EDITABLE(app->ramp_entry)) :
                              "@%#*+=-:. ");
    if (g_utf8_strlen(settings->ramp, -1) < 2) {
        g_free(settings->ramp);
        settings->ramp = g_strdup("@%#*+=-:. ");
    }
    settings->ramp_levels = CLAMP((gint)g_utf8_strlen(settings->ramp, -1), 2, 10);
    build_ramp_glyphs(settings);

    settings->font_family = g_strdup("monospace");
    font_item = app->font_dropdown != NULL ?
                gtk_drop_down_get_selected_item(app->font_dropdown) : NULL;
    if (font_item != NULL && GTK_IS_STRING_OBJECT(font_item)) {
        const gchar *font_name = gtk_string_object_get_string(GTK_STRING_OBJECT(font_item));
        if (font_name != NULL && *font_name != '\0') {
            g_free(settings->font_family);
            settings->font_family = g_strdup(font_name);
        }
    }
}

static gfloat clamp_unit(gfloat value)
{
    return CLAMP(value, 0.0f, 1.0f);
}

static GdkTexture *texture_from_rgba(guint8 *pixels,
                                     gint width,
                                     gint height,
                                     gint stride,
                                     gsize size)
{
    GBytes *bytes = g_bytes_new_take(pixels, size);
    GdkTexture *texture = GDK_TEXTURE(gdk_memory_texture_new(width,
                                                              height,
                                                              GDK_MEMORY_R8G8B8A8,
                                                              bytes,
                                                              stride));
    g_bytes_unref(bytes);
    return texture;
}

static guint8 *cpu_scale_source(const VideoFrame *frame,
                                gint width,
                                gint height,
                                gsize *out_size)
{
    guint8 *output = g_malloc((gsize)width * (gsize)height * 4);

    for (gint y = 0; y < height; y++) {
        gint source_y = MIN(frame->height - 1,
                            (gint)((gdouble)y * frame->height / MAX(1, height)));
        for (gint x = 0; x < width; x++) {
            gint source_x = MIN(frame->width - 1,
                                (gint)((gdouble)x * frame->width / MAX(1, width)));
            const guint8 *source = frame->pixels + source_y * frame->stride + source_x * 4;
            guint8 *destination = output + ((gsize)y * (gsize)width + (gsize)x) * 4;
            memcpy(destination, source, 4);
        }
    }
    *out_size = (gsize)width * (gsize)height * 4;
    return output;
}

static void get_preview_dimensions(GtkWidget *widget,
                                   gint source_width,
                                   gint source_height,
                                   gint *out_width,
                                   gint *out_height)
{
    gint width = widget != NULL ? gtk_widget_get_width(widget) : 960;
    gint height = widget != NULL ? gtk_widget_get_height(widget) : 640;

    width = CLAMP(width, 320, 960);
    height = CLAMP(height, 180, 640);
    if ((gdouble)width / height >
        (gdouble)source_width / MAX(1, source_height)) {
        width = MAX(1, (gint)round((gdouble)height * source_width /
                                  MAX(1, source_height)));
    } else {
        height = MAX(1, (gint)round((gdouble)width * source_height /
                                    MAX(1, source_width)));
    }
    *out_width = width;
    *out_height = height;
}

static void sync_custom_resolution_to_preview(AppState *app,
                                              gint width,
                                              gint height)
{
    if (app->custom_width_spin == NULL || app->custom_height_spin == NULL ||
        app->custom_resolution_user_set || width <= 0 || height <= 0) {
        return;
    }
    app->syncing_custom_resolution = TRUE;
    gtk_spin_button_set_value(app->custom_width_spin, width);
    gtk_spin_button_set_value(app->custom_height_spin, height);
    app->syncing_custom_resolution = FALSE;
}

static void copy_cairo_argb_to_rgba(cairo_surface_t *surface,
                                     guint8 *output,
                                     gint width,
                                     gint height)
{
    guint8 *source_pixels = cairo_image_surface_get_data(surface);
    gint source_stride = cairo_image_surface_get_stride(surface);

    cairo_surface_flush(surface);
    for (gint y = 0; y < height; y++) {
        for (gint x = 0; x < width; x++) {
            guint32 packed;
            guint8 *source = source_pixels + y * source_stride + x * 4;
            guint8 *destination = output + ((gsize)y * (gsize)width + (gsize)x) * 4;

            memcpy(&packed, source, sizeof(packed));
            destination[0] = (packed >> 16) & 0xff;
            destination[1] = (packed >> 8) & 0xff;
            destination[2] = packed & 0xff;
            destination[3] = packed >> 24;
        }
    }
}

static void fill_rgba_background(guint8 *output,
                                 gsize size,
                                 const gfloat background[3])
{
    guint8 color[4] = {
        (guint8)roundf(clamp_unit(background[0]) * 255.0f),
        (guint8)roundf(clamp_unit(background[1]) * 255.0f),
        (guint8)roundf(clamp_unit(background[2]) * 255.0f),
        255
    };

    for (gsize offset = 0; offset < size; offset += 4) {
        memcpy(output + offset, color, sizeof(color));
    }
}

static void ramp_character_at(const RenderSettings *settings,
                              gint index,
                              gchar output[8])
{
    const gchar *cursor = g_utf8_offset_to_pointer(settings->ramp, index);
    gint length = g_unichar_to_utf8(g_utf8_get_char(cursor), output);

    output[length] = '\0';
}

static guint8 *cpu_render_ascii(const VideoFrame *frame,
                                const RenderSettings *settings,
                                gint requested_width,
                                gint requested_height,
                                gint *out_width,
                                gint *out_height,
                                gsize *out_size)
{
    gint width = requested_width > 0 ? requested_width : frame->width;
    gint height = requested_height > 0 ? requested_height : frame->height;
    gint cell_width = MAX(1, (gint)round((gdouble)settings->cell_width * settings->glyph_aspect));
    gint cell_height = MAX(1, settings->cell_height);
    gint gap_x = cell_width >= 3 ? MAX(1, cell_width / 6) : 0;
    gint gap_y = cell_height >= 6 ? MAX(1, cell_height / 8) : 0;
    gint glyph_width = MAX(1, cell_width - gap_x);
    gint glyph_height = MAX(1, cell_height - gap_y);
    gsize size = (gsize)width * (gsize)height * 4;
    guint8 *output = g_malloc(size);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                            width,
                                                            height);
    cairo_t *cr = cairo_create(surface);

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS ||
        cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        fill_rgba_background(output, size, settings->background);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        *out_width = width;
        *out_height = height;
        *out_size = size;
        return output;
    }

    cairo_set_source_rgb(cr,
                         clamp_unit(settings->background[0]),
                         clamp_unit(settings->background[1]),
                         clamp_unit(settings->background[2]));
    cairo_paint(cr);
    cairo_select_font_face(cr,
                           settings->font_family != NULL ? settings->font_family : "monospace",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, MAX(1.0, glyph_height * 0.9));

    for (gint cell_y = 0; cell_y * cell_height < height; cell_y++) {
        gint sample_y = MIN(frame->height - 1,
                            (gint)(((gdouble)(cell_y * cell_height + cell_height / 2) /
                                    MAX(1, height)) * frame->height));
        for (gint cell_x = 0; cell_x * cell_width < width; cell_x++) {
            gint sample_x = MIN(frame->width - 1,
                                (gint)(((gdouble)(cell_x * cell_width + cell_width / 2) /
                                        MAX(1, width)) * frame->width));
            const guint8 *source = frame->pixels + sample_y * frame->stride + sample_x * 4;
            gfloat color[3] = {source[0] / 255.0f, source[1] / 255.0f, source[2] / 255.0f};
            gfloat luminance = color[0] * 0.2126f + color[1] * 0.7152f + color[2] * 0.0722f;
            gfloat ink[3];
            gint ramp_index;
            gchar glyph[8];
            cairo_text_extents_t extents;

            color[0] = clamp_unit(color[0] + (color[0] - luminance) * settings->edge);
            color[1] = clamp_unit(color[1] + (color[1] - luminance) * settings->edge);
            color[2] = clamp_unit(color[2] + (color[2] - luminance) * settings->edge);
            color[0] = clamp_unit(luminance + (color[0] - luminance) * settings->saturation);
            color[1] = clamp_unit(luminance + (color[1] - luminance) * settings->saturation);
            color[2] = clamp_unit(luminance + (color[2] - luminance) * settings->saturation);
            luminance = color[0] * 0.2126f + color[1] * 0.7152f + color[2] * 0.0722f;
            luminance = clamp_unit((luminance - 0.5f) * settings->contrast + 0.5f + settings->brightness);
            luminance = powf(luminance, MAX(0.01f, settings->gamma));
            if (settings->threshold_enabled) {
                luminance = luminance >= settings->threshold ? 1.0f : 0.0f;
            }
            if (settings->invert) {
                luminance = 1.0f - luminance;
            }
            ramp_index = CLAMP((gint)floorf((1.0f - luminance) *
                                            (settings->ramp_levels - 1)),
                               0,
                               settings->ramp_levels - 1);
            ramp_character_at(settings, ramp_index, glyph);
            memcpy(ink, settings->foreground, sizeof(ink));
            if (settings->color_mode == ASCII_COLOR_SOURCE) {
                memcpy(ink, color, sizeof(ink));
            } else if (settings->color_mode == ASCII_COLOR_PALETTE) {
                memcpy(ink, luminance < 0.34f ? settings->palette[0] :
                       luminance < 0.67f ? settings->palette[1] : settings->palette[2],
                       sizeof(ink));
            }

            cairo_text_extents(cr, glyph, &extents);
            cairo_set_source_rgb(cr,
                                 clamp_unit(ink[0] * (0.72f + luminance * 0.38f)),
                                 clamp_unit(ink[1] * (0.72f + luminance * 0.38f)),
                                 clamp_unit(ink[2] * (0.72f + luminance * 0.38f)));
            cairo_move_to(cr,
                          cell_x * cell_width + (glyph_width - extents.width) * 0.5 - extents.x_bearing,
                          cell_y * cell_height + (glyph_height - extents.height) * 0.5 - extents.y_bearing);
            cairo_show_text(cr, glyph);
        }
    }

    copy_cairo_argb_to_rgba(surface, output, width, height);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    *out_width = width;
    *out_height = height;
    *out_size = size;
    return output;
}

static void free_render_result(RenderResult *result)
{
    if (result == NULL) {
        return;
    }
    g_free(result->source_pixels);
    g_free(result->ascii_pixels);
    g_free(result);
}

static gboolean present_render_result(gpointer user_data)
{
    AppState *app = user_data;
    RenderResult *result;
    GdkTexture *source_texture;
    GdkTexture *ascii_texture;

    g_mutex_lock(&app->render_lock);
    result = app->render_result;
    app->render_result = NULL;
    app->render_present_source_id = 0;
    g_mutex_unlock(&app->render_lock);

    if (result == NULL) {
        return G_SOURCE_REMOVE;
    }

    source_texture = texture_from_rgba(result->source_pixels,
                                       result->width,
                                       result->height,
                                       result->width * 4,
                                       result->size);
    result->source_pixels = NULL;
    ascii_texture = texture_from_rgba(result->ascii_pixels,
                                      result->width,
                                      result->height,
                                      result->width * 4,
                                      result->size);
    result->ascii_pixels = NULL;
    if (app->source_picture != NULL) {
        gtk_picture_set_paintable(app->source_picture,
                                  GDK_PAINTABLE(source_texture));
    }
    if (app->ascii_picture != NULL) {
        gtk_picture_set_paintable(app->ascii_picture,
                                  GDK_PAINTABLE(ascii_texture));
    }
    app->texture_width = result->source_width;
    app->texture_height = result->source_height;
    app->preview_width = result->width;
    app->preview_height = result->height;
    sync_custom_resolution_to_preview(app, result->width, result->height);
    if (app->source_meta_label != NULL) {
        gchar *dimensions = g_strdup_printf("%dx%d",
                                            result->source_width,
                                            result->source_height);
        gtk_label_set_text(app->source_meta_label, dimensions);
        g_free(dimensions);
    }
    g_object_unref(source_texture);
    g_object_unref(ascii_texture);
    free_render_result(result);
    return G_SOURCE_REMOVE;
}

static gpointer render_worker_main(gpointer user_data)
{
    AppState *app = user_data;
    VideoFrame *frame = NULL;

    for (;;) {
        RenderSettings settings;
        RenderResult *result;
        gint preview_width;
        gint preview_height;
        gsize size;

        memset(&settings, 0, sizeof(settings));
        g_mutex_lock(&app->render_lock);
        while (!app->render_stop && !app->render_pending) {
            g_cond_wait(&app->render_cond, &app->render_lock);
        }
        if (app->render_stop) {
            g_mutex_unlock(&app->render_lock);
            break;
        }
        app->render_pending = FALSE;
        if (app->worker_settings_ready) {
            copy_render_settings(&settings, &app->worker_settings);
        }
        g_mutex_unlock(&app->render_lock);

        g_mutex_lock(&app->frame_lock);
        if (app->latest_frame != NULL) {
            free_video_frame(frame);
            frame = app->latest_frame;
            app->latest_frame = NULL;
        }
        g_mutex_unlock(&app->frame_lock);

        if (frame == NULL || settings.ramp == NULL) {
            clear_render_settings(&settings);
            continue;
        }

        get_preview_dimensions(NULL,
                               frame->width,
                               frame->height,
                               &preview_width,
                               &preview_height);
        result = g_new0(RenderResult, 1);
        result->source_pixels = cpu_scale_source(frame,
                                                 preview_width,
                                                 preview_height,
                                                 &size);
        result->ascii_pixels = cpu_render_ascii(frame,
                                                &settings,
                                                preview_width,
                                                preview_height,
                                                &result->width,
                                                &result->height,
                                                &result->size);
        result->source_width = frame->width;
        result->source_height = frame->height;
        result->size = size;

        g_mutex_lock(&app->render_lock);
        if (app->render_stop) {
            g_mutex_unlock(&app->render_lock);
            free_render_result(result);
            clear_render_settings(&settings);
            break;
        }
        free_render_result(app->render_result);
        app->render_result = result;
        if (app->render_present_source_id == 0) {
            app->render_present_source_id = g_idle_add(present_render_result, app);
        }
        g_mutex_unlock(&app->render_lock);
        clear_render_settings(&settings);
    }

    free_video_frame(frame);
    return NULL;
}

static void start_render_worker(AppState *app)
{
    RenderSettings settings;

    memset(&settings, 0, sizeof(settings));
    read_render_settings(app, &settings);
    g_mutex_lock(&app->render_lock);
    copy_render_settings(&app->worker_settings, &settings);
    app->worker_settings_ready = TRUE;
    g_mutex_unlock(&app->render_lock);
    clear_render_settings(&settings);
    app->render_thread = g_thread_new("ascii-render", render_worker_main, app);
}

static void stop_render_worker(AppState *app)
{
    RenderResult *result;
    guint source_id;

    if (app->render_thread == NULL) {
        return;
    }
    g_mutex_lock(&app->render_lock);
    app->render_stop = TRUE;
    g_cond_signal(&app->render_cond);
    g_mutex_unlock(&app->render_lock);
    g_thread_join(app->render_thread);
    app->render_thread = NULL;

    g_mutex_lock(&app->render_lock);
    source_id = app->render_present_source_id;
    app->render_present_source_id = 0;
    result = app->render_result;
    app->render_result = NULL;
    clear_render_settings(&app->worker_settings);
    g_mutex_unlock(&app->render_lock);
    if (source_id != 0) {
        g_source_remove(source_id);
    }
    free_render_result(result);
}

static void activate_cpu_fallback(AppState *app)
{
    if (app->cpu_fallback) {
        if (app->ascii_picture != NULL) {
            gtk_widget_set_visible(GTK_WIDGET(app->ascii_picture), TRUE);
        }
        if (app->source_picture != NULL) {
            gtk_widget_set_visible(GTK_WIDGET(app->source_picture), TRUE);
        }
        return;
    }
    app->cpu_fallback = TRUE;
    if (app->gl_area != NULL) {
        gtk_widget_set_visible(GTK_WIDGET(app->gl_area), FALSE);
    }
    if (app->source_gl_area != NULL) {
        gtk_widget_set_visible(GTK_WIDGET(app->source_gl_area), FALSE);
    }
    if (app->ascii_picture != NULL) {
        gtk_widget_set_visible(GTK_WIDGET(app->ascii_picture), TRUE);
    }
    if (app->source_picture != NULL) {
        gtk_widget_set_visible(GTK_WIDGET(app->source_picture), TRUE);
    }
    set_status(app, "OpenGL unavailable — using CPU preview");
}

static void queue_preview_render(AppState *app)
{
    RenderSettings settings;

    memset(&settings, 0, sizeof(settings));
    read_render_settings(app, &settings);
    g_mutex_lock(&app->render_lock);
    copy_render_settings(&app->worker_settings, &settings);
    app->worker_settings_ready = TRUE;
    app->render_pending = TRUE;
    g_cond_signal(&app->render_cond);
    g_mutex_unlock(&app->render_lock);
    clear_render_settings(&settings);
}

static void clear_latest_frame(AppState *app)
{
    VideoFrame *frame;
    VideoFrame *source_frame;

    g_mutex_lock(&app->frame_lock);
    frame = app->latest_frame;
    app->latest_frame = NULL;
    source_frame = app->latest_source_frame;
    app->latest_source_frame = NULL;
    g_mutex_unlock(&app->frame_lock);

    free_video_frame(frame);
    free_video_frame(source_frame);
}

static GstFlowReturn on_new_sample(GstAppSink *sink, gpointer user_data)
{
    AppState *app = user_data;
    GstSample *sample = gst_app_sink_pull_sample(sink);
    GstCaps *caps;
    GstVideoInfo info;
    GstBuffer *buffer;
    GstMapInfo map;
    VideoFrame *frame;

    if (sample == NULL) {
        return GST_FLOW_ERROR;
    }

    caps = gst_sample_get_caps(sample);
    buffer = gst_sample_get_buffer(sample);
    if (caps == NULL || buffer == NULL || !gst_video_info_from_caps(&info, caps)) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    frame = g_new0(VideoFrame, 1);
    frame->width = GST_VIDEO_INFO_WIDTH(&info);
    frame->height = GST_VIDEO_INFO_HEIGHT(&info);
    frame->stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
    frame->size = map.size;
    frame->pixels = g_malloc(frame->size);
    memcpy(frame->pixels, map.data, frame->size);

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    g_mutex_lock(&app->frame_lock);
    free_video_frame(app->latest_frame);
    app->latest_frame = frame;
    g_mutex_unlock(&app->frame_lock);

    g_mutex_lock(&app->render_lock);
    app->render_pending = TRUE;
    g_cond_signal(&app->render_cond);
    g_mutex_unlock(&app->render_lock);
    return GST_FLOW_OK;
}

static gboolean on_bus_message(GstBus *bus, GstMessage *message, gpointer user_data)
{
    AppState *app = user_data;
    GError *error = NULL;
    gchar *debug = NULL;

    (void)bus;

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
        gst_message_parse_error(message, &error, &debug);
        g_warning("GStreamer error: %s%s%s",
                  error != NULL ? error->message : "unknown error",
                  debug != NULL ? " (" : "",
                  debug != NULL ? debug : "");
        g_clear_error(&error);
        g_free(debug);
        app->playing = FALSE;
        if (app->play_button != NULL) {
            gtk_toggle_button_set_active(app->play_button, FALSE);
        }
        set_status(app, "Could not play this video");
        return G_SOURCE_CONTINUE;

    case GST_MESSAGE_EOS:
        if (app->loop && app->pipeline != NULL) {
            gst_element_seek_simple(app->pipeline,
                                    GST_FORMAT_TIME,
                                    GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
                                    0);
            gst_element_set_state(app->pipeline, GST_STATE_PLAYING);
            set_status(app, "Looping");
        } else {
            app->playing = FALSE;
            if (app->play_button != NULL) {
                gtk_toggle_button_set_active(app->play_button, FALSE);
            }
            set_status(app, "Playback finished");
        }
        return G_SOURCE_CONTINUE;

    default:
        return G_SOURCE_CONTINUE;
    }
}

static void stop_pipeline(AppState *app)
{
    if (app->exporting) {
        stop_export(app, FALSE);
    }
    if (app->position_timer_id != 0) {
        g_source_remove(app->position_timer_id);
        app->position_timer_id = 0;
    }

    if (app->bus_watch_id != 0) {
        g_source_remove(app->bus_watch_id);
        app->bus_watch_id = 0;
    }

    if (app->pipeline != NULL) {
        gst_element_set_state(app->pipeline, GST_STATE_NULL);
        gst_object_unref(app->pipeline);
        app->pipeline = NULL;
    }

    clear_latest_frame(app);
    app->duration = GST_CLOCK_TIME_NONE;
    app->playing = FALSE;
    app->texture_width = 0;
    app->texture_height = 0;
    app->preview_width = 0;
    app->preview_height = 0;
    app->source_texture_width = 0;
    app->source_texture_height = 0;
    g_clear_pointer(&app->video_path, g_free);
    if (app->play_button != NULL) {
        gtk_toggle_button_set_active(app->play_button, FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(app->play_button), FALSE);
    }
    if (app->progress_scale != NULL) {
        gtk_range_set_value(GTK_RANGE(app->progress_scale), 0.0);
    }
    if (app->position_label != NULL) {
        gtk_label_set_text(app->position_label, "00:00");
    }
    if (app->duration_label != NULL) {
        gtk_label_set_text(app->duration_label, "00:00");
    }
}

static gboolean update_position(gpointer user_data)
{
    AppState *app = user_data;
    gint64 position = GST_CLOCK_TIME_NONE;

    if (app->pipeline == NULL) {
        return G_SOURCE_CONTINUE;
    }

    if (gst_element_query_position(app->pipeline, GST_FORMAT_TIME, &position) &&
        gst_element_query_duration(app->pipeline, GST_FORMAT_TIME, &app->duration) &&
        app->duration > 0 && position >= 0) {
        gchar *position_text = g_strdup_printf("%02" G_GINT64_FORMAT ":%02" G_GINT64_FORMAT,
                                               position / GST_SECOND / 60,
                                               (position / GST_SECOND) % 60);
        gchar *duration_text = g_strdup_printf("%02" G_GINT64_FORMAT ":%02" G_GINT64_FORMAT,
                                               app->duration / GST_SECOND / 60,
                                               (app->duration / GST_SECOND) % 60);
        app->updating_position = TRUE;
        gtk_range_set_value(GTK_RANGE(app->progress_scale),
                            (gdouble)position / (gdouble)app->duration);
        app->updating_position = FALSE;
        gtk_label_set_text(app->position_label, position_text);
        gtk_label_set_text(app->duration_label, duration_text);
        g_free(position_text);
        g_free(duration_text);
    }

    return G_SOURCE_CONTINUE;
}

static void on_seek_changed(GtkRange *range, gpointer user_data)
{
    AppState *app = user_data;
    gdouble fraction;
    gint64 position;

    if (app->updating_position || app->pipeline == NULL || app->duration <= 0) {
        return;
    }

    fraction = gtk_range_get_value(range);
    position = (gint64)(fraction * (gdouble)app->duration);
    gst_element_seek_simple(app->pipeline,
                            GST_FORMAT_TIME,
                            GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
                            position);
}

static void on_play_toggled(GtkToggleButton *button, gpointer user_data)
{
    AppState *app = user_data;
    gboolean active = gtk_toggle_button_get_active(button);

    if (app->pipeline == NULL) {
        gtk_toggle_button_set_active(button, FALSE);
        return;
    }

    if (active) {
        gst_element_set_state(app->pipeline, GST_STATE_PLAYING);
        app->playing = TRUE;
        gtk_button_set_label(GTK_BUTTON(button), "Pause");
        set_status(app, "Playing");
    } else {
        gst_element_set_state(app->pipeline, GST_STATE_PAUSED);
        app->playing = FALSE;
        gtk_button_set_label(GTK_BUTTON(button), "Play");
        set_status(app, "Paused");
    }
}

static void apply_playback_rate(AppState *app)
{
    gint64 position = 0;

    if (app->pipeline == NULL) {
        return;
    }

    gst_element_query_position(app->pipeline, GST_FORMAT_TIME, &position);
    gst_element_seek(app->pipeline,
                     app->playback_rate,
                     GST_FORMAT_TIME,
                     GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
                     GST_SEEK_TYPE_SET,
                     position,
                     GST_SEEK_TYPE_NONE,
                     GST_CLOCK_TIME_NONE);
}

static void on_speed_selected(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    AppState *app = user_data;
    guint selected;

    (void)object;
    (void)pspec;
    selected = gtk_drop_down_get_selected(app->speed_dropdown);
    app->playback_rate = selected == 0 ? 0.5 : selected == 1 ? 1.0 : selected == 2 ? 1.5 : 2.0;
    apply_playback_rate(app);
}

static void on_loop_toggled(GtkCheckButton *button, gpointer user_data)
{
    AppState *app = user_data;

    app->loop = gtk_check_button_get_active(button);
}

static void on_invert_toggled(GtkCheckButton *button, gpointer user_data)
{
    AppState *app = user_data;

    app->invert = gtk_check_button_get_active(button);
    queue_preview_render(app);
}

static void on_color_mode_selected(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    AppState *app = user_data;

    (void)object;
    (void)pspec;
    app->color_mode = (AsciiColorMode)gtk_drop_down_get_selected(app->color_mode_dropdown);
    queue_preview_render(app);
}

static void on_ramp_changed(GtkEditable *editable, gpointer user_data)
{
    AppState *app = user_data;
    const gchar *ramp = gtk_editable_get_text(editable);

    app->ramp_levels = CLAMP((gint)g_utf8_strlen(ramp, -1), 2, 10);
    queue_preview_render(app);
}

static void on_text_setting_changed(GtkEditable *editable, gpointer user_data)
{
    AppState *app = user_data;

    (void)editable;
    queue_preview_render(app);
}

static void on_font_selected(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    AppState *app = user_data;

    (void)object;
    (void)pspec;
    queue_preview_render(app);
}

static void on_cell_spin_changed(GtkSpinButton *button, gpointer user_data)
{
    AppState *app = user_data;
    gint value = (gint)gtk_spin_button_get_value(button);

    if (button == app->ascii_size_spin) {
        app->cell_width = value;
        app->cell_height = value;
    } else if (button == app->custom_width_spin ||
               button == app->custom_height_spin) {
        return;
    }
    queue_preview_render(app);
}

static void on_custom_resolution_changed(GtkSpinButton *button, gpointer user_data)
{
    AppState *app = user_data;

    (void)button;
    if (!app->syncing_custom_resolution) {
        app->custom_resolution_user_set = TRUE;
    }
}

static void on_export_resolution_selected(GObject *object,
                                          GParamSpec *pspec,
                                          gpointer user_data)
{
    AppState *app = user_data;
    guint selected;

    (void)object;
    (void)pspec;
    selected = gtk_drop_down_get_selected(app->resolution_dropdown);
    if (app->custom_resolution_grid != NULL) {
        gtk_widget_set_visible(app->custom_resolution_grid, selected == 1);
    }
}

static void on_setting_scale_changed(GtkRange *range, gpointer user_data)
{
    AppState *app = user_data;
    GtkWidget *widget = GTK_WIDGET(range);
    gdouble value = gtk_range_get_value(range);

    if (widget == GTK_WIDGET(app->glyph_aspect_scale)) {
        app->glyph_aspect = (gfloat)value;
    } else if (widget == GTK_WIDGET(app->brightness_scale)) {
        app->brightness = (gfloat)value;
    } else if (widget == GTK_WIDGET(app->contrast_scale)) {
        app->contrast = (gfloat)value;
    } else if (widget == GTK_WIDGET(app->gamma_scale)) {
        app->gamma = (gfloat)value;
    } else if (widget == GTK_WIDGET(app->saturation_scale)) {
        app->saturation = (gfloat)value;
    } else if (widget == GTK_WIDGET(app->edge_scale)) {
        app->edge = (gfloat)value;
    } else if (widget == GTK_WIDGET(app->threshold_scale)) {
        app->threshold = (gfloat)value;
    }
    queue_preview_render(app);
}

static void on_threshold_selected(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    AppState *app = user_data;

    (void)object;
    (void)pspec;
    app->threshold_enabled = gtk_drop_down_get_selected(app->threshold_dropdown) != 0;
    queue_preview_render(app);
}

static void on_preset_selected(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    static const PresetValues presets[] = {
        {
            "@%#*+=-:. ", ASCII_COLOR_GRAYSCALE,
            "#f5f5f0", "#11110f", "#f8fafc, #94a3b8, #0f0f23",
            0.0, 1.18, 0.92, 1.0, 0.0, 0.5, FALSE, FALSE
        },
        {
            "@#+=-:. ", ASCII_COLOR_GRAYSCALE,
            "#ffffff", "#000000", "#ffffff, #bdbdbd, #202020",
            0.0, 1.55, 0.80, 1.0, 0.0, 0.5, FALSE, FALSE
        },
        {
            "@%#*+=-:. ", ASCII_COLOR_PALETTE,
            "#44ff88", "#06130d", "#06130d, #0b6b3a, #44ff88",
            0.0, 1.18, 0.92, 1.0, 0.0, 0.5, FALSE, FALSE
        },
        {
            "@&B9#SGHMh253AXsri;:,. ", ASCII_COLOR_GRAYSCALE,
            "#ffb000", "#1a0c00", "#fff0b3, #c46b00, #3b1400",
            0.0, 1.18, 1.10, 0.0, 0.0, 0.5, FALSE, FALSE
        },
        {
            "@%#*+:.· ", ASCII_COLOR_SOURCE,
            "#f5f5f0", "#11110f", "#f8fafc, #94a3b8, #0f0f23",
            0.0, 1.00, 0.92, 1.0, 0.0, 0.5, FALSE, FALSE
        }
    };
    AppState *app = user_data;
    const PresetValues *preset;
    guint selected;

    (void)object;
    (void)pspec;
    selected = gtk_drop_down_get_selected(app->preset_dropdown);
    if (selected >= G_N_ELEMENTS(presets) || app->ramp_entry == NULL) {
        return;
    }
    preset = &presets[selected];
    gtk_editable_set_text(GTK_EDITABLE(app->ramp_entry), preset->ramp);
    gtk_drop_down_set_selected(app->color_mode_dropdown, preset->color_mode);
    gtk_editable_set_text(GTK_EDITABLE(app->foreground_entry), preset->foreground);
    gtk_editable_set_text(GTK_EDITABLE(app->background_entry), preset->background);
    gtk_editable_set_text(GTK_EDITABLE(app->palette_entry), preset->palette);
    gtk_range_set_value(GTK_RANGE(app->brightness_scale), preset->brightness);
    gtk_range_set_value(GTK_RANGE(app->contrast_scale), preset->contrast);
    gtk_range_set_value(GTK_RANGE(app->gamma_scale), preset->gamma);
    gtk_range_set_value(GTK_RANGE(app->saturation_scale), preset->saturation);
    gtk_range_set_value(GTK_RANGE(app->edge_scale), preset->edge);
    gtk_range_set_value(GTK_RANGE(app->threshold_scale), preset->threshold);
    gtk_drop_down_set_selected(app->threshold_dropdown, preset->threshold_enabled ? 1 : 0);
    gtk_check_button_set_active(app->invert_button, preset->invert);
    queue_preview_render(app);
}

static GLuint compile_shader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    GLint compiled = GL_FALSE;
    GLint log_length = 0;

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
        gchar *log = g_malloc0((gsize)log_length + 1);
        glGetShaderInfoLog(shader, log_length, NULL, log);
        g_warning("GLSL shader compilation failed: %s", log);
        g_free(log);
    }
    glDeleteShader(shader);
    return 0;
}

static GLuint create_shader_program(void)
{
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    GLuint program;
    GLint linked = GL_FALSE;
    GLint log_length = 0;

    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) {
            glDeleteShader(vertex);
        }
        if (fragment != 0) {
            glDeleteShader(fragment);
        }
        return 0;
    }

    program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) {
        return program;
    }

    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
        gchar *log = g_malloc0((gsize)log_length + 1);
        glGetProgramInfoLog(program, log_length, NULL, log);
        g_warning("GLSL program link failed: %s", log);
        g_free(log);
    }
    glDeleteProgram(program);
    return 0;
}

static void on_gl_realize(GtkGLArea *area, gpointer user_data)
{
    AppState *app = user_data;
    const GLubyte *version;
    const GLubyte *vendor;
    const GLubyte *renderer;

    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area) != NULL) {
        g_warning("OpenGL context unavailable; using CPU preview");
        activate_cpu_fallback(app);
        return;
    }

    version = glGetString(GL_VERSION);
    vendor = glGetString(GL_VENDOR);
    renderer = glGetString(GL_RENDERER);
    g_message("ASCII renderer using OpenGL %s (%s / %s)",
              version != NULL ? (const char *)version : "unknown",
              vendor != NULL ? (const char *)vendor : "unknown vendor",
              renderer != NULL ? (const char *)renderer : "unknown renderer");

    app->shader_program = create_shader_program();
    if (app->shader_program == 0) {
        activate_cpu_fallback(app);
        return;
    }

    glGenVertexArrays(1, &app->vertex_array);
    glBindVertexArray(app->vertex_array);

    glGenTextures(1, &app->video_texture);
    glBindTexture(GL_TEXTURE_2D, app->video_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    app->video_uniform = glGetUniformLocation(app->shader_program, "u_video");
    app->video_size_uniform = glGetUniformLocation(app->shader_program, "u_video_size");
    app->viewport_size_uniform = glGetUniformLocation(app->shader_program, "u_viewport_size");
    app->cell_size_uniform = glGetUniformLocation(app->shader_program, "u_cell_size");
    app->tint_uniform = glGetUniformLocation(app->shader_program, "u_tint");
    app->contrast_uniform = glGetUniformLocation(app->shader_program, "u_contrast");
    app->gamma_uniform = glGetUniformLocation(app->shader_program, "u_gamma");
    app->brightness_uniform = glGetUniformLocation(app->shader_program, "u_brightness");
    app->saturation_uniform = glGetUniformLocation(app->shader_program, "u_saturation");
    app->edge_uniform = glGetUniformLocation(app->shader_program, "u_edge");
    app->invert_uniform = glGetUniformLocation(app->shader_program, "u_invert");
    app->threshold_enabled_uniform = glGetUniformLocation(app->shader_program, "u_threshold_enabled");
    app->threshold_uniform = glGetUniformLocation(app->shader_program, "u_threshold");
    app->ramp_levels_uniform = glGetUniformLocation(app->shader_program, "u_ramp_levels");
    app->foreground_uniform = glGetUniformLocation(app->shader_program, "u_foreground");
    app->background_uniform = glGetUniformLocation(app->shader_program, "u_background");
    app->palette0_uniform = glGetUniformLocation(app->shader_program, "u_palette0");
    app->palette1_uniform = glGetUniformLocation(app->shader_program, "u_palette1");
    app->palette2_uniform = glGetUniformLocation(app->shader_program, "u_palette2");
    app->color_mode_uniform = glGetUniformLocation(app->shader_program, "u_color_mode");
    app->gl_ready = TRUE;
}

static void upload_latest_frame(AppState *app)
{
    VideoFrame *frame;
    gboolean new_texture;

    g_mutex_lock(&app->frame_lock);
    frame = app->latest_frame;
    app->latest_frame = NULL;
    g_mutex_unlock(&app->frame_lock);

    if (frame == NULL) {
        return;
    }

    new_texture = app->texture_width != frame->width || app->texture_height != frame->height;
    glBindTexture(GL_TEXTURE_2D, app->video_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->stride / 4);

    if (new_texture) {
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA8,
                     frame->width,
                     frame->height,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     frame->pixels);
        app->texture_width = frame->width;
        app->texture_height = frame->height;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        frame->width,
                        frame->height,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        frame->pixels);
    }

    if (app->source_meta_label != NULL) {
        gchar *dimensions = g_strdup_printf("%dx%d", frame->width, frame->height);
        gtk_label_set_text(app->source_meta_label, dimensions);
        g_free(dimensions);
    }

    if (app->source_picture != NULL) {
        gint preview_width;
        gint preview_height;
        gsize preview_size;
        guint8 *preview_pixels;
        GdkTexture *preview_texture;

        get_preview_dimensions(GTK_WIDGET(app->source_picture),
                               frame->width,
                               frame->height,
                               &preview_width,
                               &preview_height);
        preview_pixels = cpu_scale_source(frame,
                                          preview_width,
                                          preview_height,
                                          &preview_size);
        preview_texture = texture_from_rgba(preview_pixels,
                                            preview_width,
                                            preview_height,
                                            preview_width * 4,
                                            preview_size);
        gtk_picture_set_paintable(app->source_picture,
                                   GDK_PAINTABLE(preview_texture));
        g_object_unref(preview_texture);
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    free_video_frame(frame);
}

static gboolean on_gl_render(GtkGLArea *area, GdkGLContext *context, gpointer user_data)
{
    AppState *app = user_data;
    gint scale;
    gint width;
    gint height;
    gfloat cell_height;
    gfloat cell_width;
    gfloat foreground[3] = {0.96f, 0.96f, 0.96f};
    gfloat background[3] = {0.012f, 0.016f, 0.022f};
    gfloat palette[3][3];

    (void)context;

    if (!app->gl_ready) {
        glClearColor(0.012f, 0.016f, 0.022f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return TRUE;
    }

    scale = gtk_widget_get_scale_factor(GTK_WIDGET(area));
    width = gtk_widget_get_width(GTK_WIDGET(area)) * scale;
    height = gtk_widget_get_height(GTK_WIDGET(area)) * scale;
    if (width <= 0 || height <= 0) {
        return TRUE;
    }

    upload_latest_frame(app);

    glViewport(0, 0, width, height);
    glClearColor(0.012f, 0.016f, 0.022f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (app->texture_width <= 0 || app->texture_height <= 0) {
        return TRUE;
    }

    cell_height = (gfloat)app->cell_height * (gfloat)scale;
    cell_width = (gfloat)app->cell_width * (gfloat)scale * app->glyph_aspect;
    parse_hex_color(app->foreground_entry != NULL ?
                        gtk_editable_get_text(GTK_EDITABLE(app->foreground_entry)) : "#f5f5f0",
                    foreground);
    parse_hex_color(app->background_entry != NULL ?
                        gtk_editable_get_text(GTK_EDITABLE(app->background_entry)) : "#11110f",
                    background);
    parse_palette(app->palette_entry != NULL ?
                      gtk_editable_get_text(GTK_EDITABLE(app->palette_entry)) : NULL,
                  palette);

    glUseProgram(app->shader_program);
    glBindVertexArray(app->vertex_array);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, app->video_texture);
    glUniform1i(app->video_uniform, 0);
    glUniform2f(app->video_size_uniform,
                (gfloat)app->texture_width,
                (gfloat)app->texture_height);
    glUniform2f(app->viewport_size_uniform, (gfloat)width, (gfloat)height);
    glUniform2f(app->cell_size_uniform, cell_width, cell_height);
    glUniform1i(app->color_mode_uniform, app->color_mode);
    glUniform3f(app->tint_uniform, 0.35f, 0.95f, 0.72f);
    glUniform3fv(app->foreground_uniform, 1, foreground);
    glUniform3fv(app->background_uniform, 1, background);
    glUniform3fv(app->palette0_uniform, 1, palette[0]);
    glUniform3fv(app->palette1_uniform, 1, palette[1]);
    glUniform3fv(app->palette2_uniform, 1, palette[2]);
    glUniform1f(app->brightness_uniform, app->brightness);
    glUniform1f(app->contrast_uniform, app->contrast);
    glUniform1f(app->gamma_uniform, app->gamma);
    glUniform1f(app->saturation_uniform, app->saturation);
    glUniform1f(app->edge_uniform, app->edge);
    glUniform1i(app->invert_uniform, app->invert ? GL_TRUE : GL_FALSE);
    glUniform1i(app->threshold_enabled_uniform, app->threshold_enabled ? GL_TRUE : GL_FALSE);
    glUniform1f(app->threshold_uniform, app->threshold);
    glUniform1i(app->ramp_levels_uniform, app->ramp_levels);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    return TRUE;
}

static void on_gl_unrealize(GtkGLArea *area, gpointer user_data)
{
    AppState *app = user_data;

    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area) != NULL) {
        return;
    }

    if (app->video_texture != 0) {
        glDeleteTextures(1, &app->video_texture);
        app->video_texture = 0;
    }
    if (app->vertex_array != 0) {
        glDeleteVertexArrays(1, &app->vertex_array);
        app->vertex_array = 0;
    }
    if (app->shader_program != 0) {
        glDeleteProgram(app->shader_program);
        app->shader_program = 0;
    }
    app->gl_ready = FALSE;
}

static void on_source_gl_realize(GtkGLArea *area, gpointer user_data)
{
    AppState *app = user_data;

    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area) != NULL) {
        activate_cpu_fallback(app);
        return;
    }

    app->source_shader_program = 0;
    {
        GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
        GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, source_fragment_shader_source);
        GLint linked = GL_FALSE;

        if (vertex != 0 && fragment != 0) {
            app->source_shader_program = glCreateProgram();
            glAttachShader(app->source_shader_program, vertex);
            glAttachShader(app->source_shader_program, fragment);
            glLinkProgram(app->source_shader_program);
            glGetProgramiv(app->source_shader_program, GL_LINK_STATUS, &linked);
        }
        if (vertex != 0) {
            glDeleteShader(vertex);
        }
        if (fragment != 0) {
            glDeleteShader(fragment);
        }
        if (linked != GL_TRUE && app->source_shader_program != 0) {
            glDeleteProgram(app->source_shader_program);
            app->source_shader_program = 0;
        }
    }

    if (app->source_shader_program == 0) {
        activate_cpu_fallback(app);
        return;
    }

    glGenVertexArrays(1, &app->source_vertex_array);
    glBindVertexArray(app->source_vertex_array);
    glGenTextures(1, &app->source_video_texture);
    glBindTexture(GL_TEXTURE_2D, app->source_video_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    app->source_video_uniform = glGetUniformLocation(app->source_shader_program, "u_video");
    app->source_video_size_uniform = glGetUniformLocation(app->source_shader_program, "u_video_size");
    app->source_viewport_size_uniform = glGetUniformLocation(app->source_shader_program, "u_viewport_size");
}

static void upload_latest_source_frame(AppState *app)
{
    VideoFrame *frame;
    gboolean new_texture;

    g_mutex_lock(&app->frame_lock);
    frame = app->latest_source_frame;
    app->latest_source_frame = NULL;
    g_mutex_unlock(&app->frame_lock);
    if (frame == NULL) {
        return;
    }

    new_texture = app->source_texture_width != frame->width ||
                 app->source_texture_height != frame->height;
    glBindTexture(GL_TEXTURE_2D, app->source_video_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->stride / 4);
    if (new_texture) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     frame->width, frame->height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, frame->pixels);
        app->source_texture_width = frame->width;
        app->source_texture_height = frame->height;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        frame->width, frame->height,
                        GL_RGBA, GL_UNSIGNED_BYTE, frame->pixels);
    }
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    free_video_frame(frame);
}

static gboolean on_source_gl_render(GtkGLArea *area,
                                    GdkGLContext *context,
                                    gpointer user_data)
{
    AppState *app = user_data;
    gint scale;
    gint width;
    gint height;

    (void)context;

    scale = gtk_widget_get_scale_factor(GTK_WIDGET(area));
    width = gtk_widget_get_width(GTK_WIDGET(area)) * scale;
    height = gtk_widget_get_height(GTK_WIDGET(area)) * scale;
    if (width <= 0 || height <= 0) {
        return TRUE;
    }

    upload_latest_source_frame(app);
    glViewport(0, 0, width, height);
    glClearColor(0.012f, 0.016f, 0.022f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (app->source_shader_program == 0 || app->source_texture_width <= 0) {
        return TRUE;
    }

    glUseProgram(app->source_shader_program);
    glBindVertexArray(app->source_vertex_array);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, app->source_video_texture);
    glUniform1i(app->source_video_uniform, 0);
    glUniform2f(app->source_video_size_uniform,
                (gfloat)app->source_texture_width,
                (gfloat)app->source_texture_height);
    glUniform2f(app->source_viewport_size_uniform, (gfloat)width, (gfloat)height);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    return TRUE;
}

static void on_source_gl_unrealize(GtkGLArea *area, gpointer user_data)
{
    AppState *app = user_data;

    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area) != NULL) {
        return;
    }
    if (app->source_video_texture != 0) {
        glDeleteTextures(1, &app->source_video_texture);
        app->source_video_texture = 0;
    }
    if (app->source_vertex_array != 0) {
        glDeleteVertexArrays(1, &app->source_vertex_array);
        app->source_vertex_array = 0;
    }
    if (app->source_shader_program != 0) {
        glDeleteProgram(app->source_shader_program);
        app->source_shader_program = 0;
    }
}

static void load_video(AppState *app, GFile *file)
{
    gchar *path = NULL;
    gchar *uri = NULL;
    GError *error = NULL;
    GstCaps *caps;
    GstBus *bus;
    GstElement *appsink;
    GstElement *video_sink_bin;
    GstElement *video_convert;
    GstPad *video_convert_sink_pad;
    GstPad *video_sink_pad;
    gchar *basename;

    path = g_file_get_path(file);
    if (path == NULL) {
        set_status(app, "This location is not a local video file");
        return;
    }

    uri = gst_filename_to_uri(path, &error);
    if (uri == NULL) {
        set_status(app, error != NULL ? error->message : "Could not open the video");
        g_clear_error(&error);
        g_free(path);
        return;
    }

    stop_pipeline(app);
    app->video_path = g_strdup(path);
    set_status(app, "Loading video…");
    gtk_widget_set_visible(app->empty_state, FALSE);
    gtk_widget_set_visible(app->source_empty_state, FALSE);

    app->pipeline = gst_element_factory_make("playbin", "ascii_playbin");
    appsink = gst_element_factory_make("appsink", "ascii_sink");
    video_sink_bin = gst_bin_new("ascii_video_sink");
    video_convert = gst_element_factory_make("videoconvert", "ascii_video_convert");
    if (app->pipeline == NULL || appsink == NULL ||
        video_sink_bin == NULL || video_convert == NULL) {
        set_status(app, "GStreamer playbin/appsink is unavailable");
        g_clear_object(&app->pipeline);
        g_clear_object(&appsink);
        g_clear_object(&video_sink_bin);
        g_clear_object(&video_convert);
        g_free(uri);
        g_free(path);
        return;
    }

    caps = gst_caps_from_string("video/x-raw,format=RGBA");
    gst_app_sink_set_caps(GST_APP_SINK(appsink), caps);
    gst_caps_unref(caps);
    g_object_set(appsink,
                 "emit-signals", TRUE,
                 "sync", TRUE,
                 "max-buffers", 1u,
                 "drop", TRUE,
                 "enable-last-sample", FALSE,
                 NULL);
    g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), app);

    gst_bin_add_many(GST_BIN(video_sink_bin), video_convert, appsink, NULL);
    if (!gst_element_link(video_convert, appsink)) {
        set_status(app, "Could not create the video preview sink");
        gst_object_unref(video_sink_bin);
        g_clear_object(&app->pipeline);
        g_free(uri);
        g_free(path);
        return;
    }
    video_convert_sink_pad = gst_element_get_static_pad(video_convert, "sink");
    video_sink_pad = gst_ghost_pad_new("sink", video_convert_sink_pad);
    gst_object_unref(video_convert_sink_pad);
    if (video_sink_pad == NULL ||
        !gst_element_add_pad(video_sink_bin, video_sink_pad)) {
        if (video_sink_pad != NULL) {
            gst_object_unref(video_sink_pad);
        }
        set_status(app, "Could not connect the video preview sink");
        gst_object_unref(video_sink_bin);
        g_clear_object(&app->pipeline);
        g_free(uri);
        g_free(path);
        return;
    }

    g_object_set(app->pipeline, "uri", uri, "video-sink", video_sink_bin, NULL);
    gst_object_unref(video_sink_bin);

    bus = gst_element_get_bus(app->pipeline);
    app->bus_watch_id = gst_bus_add_watch(bus, on_bus_message, app);
    gst_object_unref(bus);

    if (gst_element_set_state(app->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        set_status(app, "Could not start playback");
        stop_pipeline(app);
        gtk_widget_set_visible(app->empty_state, TRUE);
        gtk_widget_set_visible(app->source_empty_state, TRUE);
        g_free(uri);
        g_free(path);
        return;
    }

    app->playing = TRUE;
    gtk_toggle_button_set_active(app->play_button, TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(app->play_button), TRUE);
    gtk_button_set_label(GTK_BUTTON(app->play_button), "Pause");
    app->duration = GST_CLOCK_TIME_NONE;
    app->position_timer_id = g_timeout_add(100, update_position, app);

    basename = g_file_get_basename(file);
    gtk_label_set_text(app->title_label, basename != NULL ? basename : "ASCII Video");
    gtk_label_set_text(app->source_meta_label, "");
    set_status(app, "Playing");
    g_free(basename);
    g_free(uri);
    g_free(path);
}

static void on_file_dialog_response(GtkNativeDialog *dialog,
                                    gint response,
                                    gpointer user_data)
{
    AppState *app = user_data;

    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
        if (file != NULL) {
            load_video(app, file);
            g_object_unref(file);
        }
    }

    gtk_native_dialog_destroy(dialog);
}

static void on_open_clicked(GtkButton *button, gpointer user_data)
{
    AppState *app = user_data;
    GtkFileChooserNative *dialog;
    GtkFileFilter *filter;

    (void)button;

    dialog = gtk_file_chooser_native_new("Open video",
                                         GTK_WINDOW(app->window),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         "Open",
                                         "Cancel");
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Video files");
    gtk_file_filter_add_mime_type(filter, "video/mp4");
    gtk_file_filter_add_mime_type(filter, "video/webm");
    gtk_file_filter_add_mime_type(filter, "video/quicktime");
    gtk_file_filter_add_mime_type(filter, "video/x-matroska");
    gtk_file_filter_add_pattern(filter, "*.avi");
    gtk_file_filter_add_pattern(filter, "*.m4v");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    g_signal_connect(dialog, "response", G_CALLBACK(on_file_dialog_response), app);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));
}

static gboolean on_window_close_request(GtkWindow *window, gpointer user_data)
{
    AppState *app = user_data;

    (void)window;
    stop_pipeline(app);
    return FALSE;
}

static void on_open_files(GApplication *application,
                          GFile **files,
                          gint n_files,
                          const gchar *hint,
                          gpointer user_data)
{
    AppState *app = user_data;

    (void)application;
    (void)hint;

    if (n_files > 0) {
        if (app->window == NULL) {
            app->application = GTK_APPLICATION(application);
            build_window(app);
        }
        load_video(app, files[0]);
    }
}

static GtkWidget *section_label(const gchar *text)
{
    GtkWidget *label = gtk_label_new(text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    return label;
}

static GtkWidget *field_label(const gchar *text)
{
    GtkWidget *label = gtk_label_new(text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    return label;
}

static gboolean show_render_gl_area(gpointer user_data)
{
    GtkWidget *gl_area = GTK_WIDGET(user_data);
    GtkWidget *overlay = gtk_widget_get_parent(gl_area);

    if (overlay == NULL) {
        g_object_unref(gl_area);
        return G_SOURCE_REMOVE;
    }
    if (gtk_widget_get_width(overlay) <= 0 ||
        gtk_widget_get_height(overlay) <= 0) {
        return G_SOURCE_CONTINUE;
    }

    gtk_widget_set_visible(gl_area, TRUE);
    g_object_unref(gl_area);
    return G_SOURCE_REMOVE;
}

static void on_render_overlay_map(GtkWidget *overlay, gpointer user_data)
{
    GtkWidget *gl_area = GTK_WIDGET(user_data);

    (void)overlay;
    g_timeout_add(250, show_render_gl_area, g_object_ref(gl_area));
}

static GtkWidget *make_dropdown(const gchar * const *items)
{
    GtkWidget *dropdown = GTK_WIDGET(gtk_drop_down_new_from_strings(items));
    gtk_widget_set_hexpand(dropdown, TRUE);
    return dropdown;
}

static GtkWidget *make_font_dropdown(void)
{
    PangoFontMap *font_map = pango_cairo_font_map_get_default();
    PangoFontFamily **families = NULL;
    gint family_count = 0;
    gchar **names;
    GtkStringList *string_list;
    GtkWidget *dropdown;
    guint selected = 0;
    gint i;

    pango_font_map_list_families(font_map, &families, &family_count);
    names = g_new0(gchar *, MAX(1, family_count) + 1);
    if (family_count == 0) {
        names[0] = g_strdup("monospace");
    } else {
        for (i = 0; i < family_count; i++) {
            names[i] = g_strdup(pango_font_family_get_name(families[i]));
            if (g_strcmp0(names[i], "JetBrains Mono") == 0) {
                selected = (guint)i;
            }
        }
    }

    string_list = gtk_string_list_new((const char * const *)names);
    dropdown = GTK_WIDGET(gtk_drop_down_new(G_LIST_MODEL(string_list), NULL));
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown), selected);
    gtk_drop_down_set_enable_search(GTK_DROP_DOWN(dropdown), TRUE);
    gtk_widget_set_hexpand(dropdown, TRUE);

    g_object_unref(string_list);
    g_strfreev(names);
    g_free(families);
    return dropdown;
}

static GtkWidget *make_scale(gdouble min,
                             gdouble max,
                             gdouble step,
                             gdouble value,
                             AppState *app)
{
    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                min, max, step);
    gtk_widget_set_hexpand(scale, TRUE);
    gtk_scale_set_draw_value(GTK_SCALE(scale), TRUE);
    gtk_range_set_value(GTK_RANGE(scale), value);
    g_signal_connect(scale, "value-changed", G_CALLBACK(on_setting_scale_changed), app);
    return scale;
}

static GtkWidget *make_spin(gdouble min, gdouble max, gdouble value, AppState *app)
{
    GtkAdjustment *adjustment = gtk_adjustment_new(value, min, max, 1.0, 4.0, 0.0);
    GtkWidget *spin = gtk_spin_button_new(adjustment, 1.0, 0);
    gtk_widget_set_hexpand(spin, TRUE);
    g_signal_connect(spin, "value-changed", G_CALLBACK(on_cell_spin_changed), app);
    return spin;
}

static void append_labeled(GtkGrid *grid,
                           gint column,
                           gint row,
                           const gchar *label_text,
                           GtkWidget *widget)
{
    GtkWidget *label = field_label(label_text);
    gtk_grid_attach(grid, label, column, row * 2, 1, 1);
    gtk_grid_attach(grid, widget, column, row * 2 + 1, 1, 1);
    gtk_widget_set_margin_bottom(label, 4);
    gtk_widget_set_margin_bottom(widget, 10);
}

static GtkWidget *build_playback_bar(AppState *app)
{
    static const gchar *speed_items[] = {"0.5x", "1x", "1.5x", "2x", NULL};
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *play;
    GtkWidget *position;
    GtkWidget *duration;
    GtkWidget *speed_label;
    GtkWidget *loop;

    gtk_widget_set_margin_top(bar, 12);
    gtk_widget_set_margin_bottom(bar, 12);
    gtk_widget_set_margin_start(bar, 22);
    gtk_widget_set_margin_end(bar, 22);
    gtk_widget_set_margin_top(bar, 12);

    play = gtk_toggle_button_new_with_label("Play");
    gtk_widget_set_sensitive(play, FALSE);
    gtk_widget_set_tooltip_text(play, "Play or pause");
    g_signal_connect(play, "toggled", G_CALLBACK(on_play_toggled), app);

    position = gtk_label_new("00:00");
    duration = gtk_label_new("00:00");
    app->position_label = GTK_LABEL(position);
    app->duration_label = GTK_LABEL(duration);

    app->progress_scale = GTK_SCALE(gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                               0.0, 1.0, 0.001));
    gtk_widget_set_hexpand(GTK_WIDGET(app->progress_scale), TRUE);
    gtk_scale_set_draw_value(app->progress_scale, FALSE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(app->progress_scale), "Seek");
    g_signal_connect(app->progress_scale, "value-changed", G_CALLBACK(on_seek_changed), app);

    speed_label = field_label("Speed");
    app->speed_dropdown = GTK_DROP_DOWN(make_dropdown(speed_items));
    gtk_drop_down_set_selected(app->speed_dropdown, 1);
    g_signal_connect(app->speed_dropdown, "notify::selected", G_CALLBACK(on_speed_selected), app);

    loop = gtk_check_button_new_with_label("Loop");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(loop), TRUE);
    g_signal_connect(loop, "toggled", G_CALLBACK(on_loop_toggled), app);
    app->loop_button = GTK_CHECK_BUTTON(loop);
    app->loop = TRUE;

    gtk_box_append(GTK_BOX(bar), play);
    gtk_box_append(GTK_BOX(bar), position);
    gtk_box_append(GTK_BOX(bar), GTK_WIDGET(app->progress_scale));
    gtk_box_append(GTK_BOX(bar), duration);
    gtk_box_append(GTK_BOX(bar), speed_label);
    gtk_box_append(GTK_BOX(bar), GTK_WIDGET(app->speed_dropdown));
    gtk_box_append(GTK_BOX(bar), loop);
    app->play_button = GTK_TOGGLE_BUTTON(play);
    return bar;
}

static GtkWidget *build_settings_panel(AppState *app)
{
    static const gchar *preset_items[] = {
        "Classic Terminal", "High Contrast", "Matrix Green", "Amber CRT", "Color Signal", NULL
    };
    static const gchar *color_items[] = {"Grayscale", "Source color", "Palette", NULL};
    static const gchar *threshold_items[] = {"Off", "Hard threshold", NULL};
    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *grid;
    GtkWidget *preset;
    GtkWidget *ramp;
    GtkWidget *color_mode;
    GtkWidget *foreground;
    GtkWidget *background;
    GtkWidget *palette;
    GtkWidget *tone_grid;
    GtkWidget *invert;
    GtkWidget *threshold;

    gtk_widget_set_margin_start(panel, 22);
    gtk_widget_set_margin_end(panel, 10);
    gtk_widget_set_margin_bottom(panel, 22);

    gtk_box_append(GTK_BOX(panel), section_label("ART DIRECTION"));
    gtk_box_append(GTK_BOX(panel), field_label("STARTING PRESET"));
    preset = make_dropdown(preset_items);
    app->preset_dropdown = GTK_DROP_DOWN(preset);
    g_signal_connect(preset, "notify::selected", G_CALLBACK(on_preset_selected), app);
    gtk_box_append(GTK_BOX(panel), preset);

    gtk_box_append(GTK_BOX(panel), section_label("CHARACTER SYSTEM"));
    grid = GTK_WIDGET(gtk_grid_new());
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_widget_set_hexpand(grid, TRUE);
    app->ascii_size_spin = GTK_SPIN_BUTTON(make_spin(4, 48, 18, app));
    append_labeled(GTK_GRID(grid), 0, 0, "ASCII SIZE (PX)", GTK_WIDGET(app->ascii_size_spin));
    gtk_box_append(GTK_BOX(panel), grid);

    gtk_box_append(GTK_BOX(panel), field_label("CHARACTER RAMP"));
    ramp = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ramp), "@%#*+=-:. ");
    g_signal_connect(ramp, "changed", G_CALLBACK(on_ramp_changed), app);
    app->ramp_entry = GTK_ENTRY(ramp);
    gtk_box_append(GTK_BOX(panel), ramp);

    gtk_box_append(GTK_BOX(panel), field_label("FONT FAMILY"));
    app->font_dropdown = GTK_DROP_DOWN(make_font_dropdown());
    g_signal_connect(app->font_dropdown,
                     "notify::selected",
                     G_CALLBACK(on_font_selected),
                     app);
    gtk_box_append(GTK_BOX(panel), GTK_WIDGET(app->font_dropdown));

    gtk_box_append(GTK_BOX(panel), field_label("CHARACTER SHAPE (WIDTH / HEIGHT)"));
    app->glyph_aspect_scale = GTK_SCALE(make_scale(0.25, 1.0, 0.05, 0.58, app));
    gtk_box_append(GTK_BOX(panel), GTK_WIDGET(app->glyph_aspect_scale));

    gtk_box_append(GTK_BOX(panel), section_label("COLOR AND TONE"));
    gtk_box_append(GTK_BOX(panel), field_label("ASCII COLOR MODE"));
    color_mode = make_dropdown(color_items);
    app->color_mode_dropdown = GTK_DROP_DOWN(color_mode);
    gtk_drop_down_set_selected(app->color_mode_dropdown, ASCII_COLOR_GRAYSCALE);
    g_signal_connect(color_mode, "notify::selected", G_CALLBACK(on_color_mode_selected), app);
    gtk_box_append(GTK_BOX(panel), color_mode);

    grid = GTK_WIDGET(gtk_grid_new());
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    foreground = gtk_entry_new();
    background = gtk_entry_new();
    palette = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(foreground), "#f5f5f0");
    gtk_editable_set_text(GTK_EDITABLE(background), "#11110f");
    gtk_editable_set_text(GTK_EDITABLE(palette), "#f8fafc, #94a3b8, #0f0f23");
    append_labeled(GTK_GRID(grid), 0, 0, "FOREGROUND", foreground);
    append_labeled(GTK_GRID(grid), 1, 0, "BACKGROUND", background);
    gtk_box_append(GTK_BOX(panel), grid);
    gtk_box_append(GTK_BOX(panel), field_label("PALETTE SWATCHES"));
    gtk_box_append(GTK_BOX(panel), palette);
    app->foreground_entry = GTK_ENTRY(foreground);
    app->background_entry = GTK_ENTRY(background);
    app->palette_entry = GTK_ENTRY(palette);
    g_signal_connect(foreground, "changed", G_CALLBACK(on_text_setting_changed), app);
    g_signal_connect(background, "changed", G_CALLBACK(on_text_setting_changed), app);
    g_signal_connect(palette, "changed", G_CALLBACK(on_text_setting_changed), app);

    tone_grid = GTK_WIDGET(gtk_grid_new());
    gtk_grid_set_column_spacing(GTK_GRID(tone_grid), 12);
    app->brightness_scale = GTK_SCALE(make_scale(-1.0, 1.0, 0.01, 0.0, app));
    app->contrast_scale = GTK_SCALE(make_scale(0.5, 2.0, 0.01, 1.18, app));
    app->gamma_scale = GTK_SCALE(make_scale(0.2, 2.5, 0.01, 0.92, app));
    app->saturation_scale = GTK_SCALE(make_scale(0.0, 2.0, 0.01, 1.0, app));
    app->edge_scale = GTK_SCALE(make_scale(0.0, 1.0, 0.01, 0.0, app));
    append_labeled(GTK_GRID(tone_grid), 0, 0, "BRIGHTNESS", GTK_WIDGET(app->brightness_scale));
    append_labeled(GTK_GRID(tone_grid), 1, 0, "CONTRAST", GTK_WIDGET(app->contrast_scale));
    append_labeled(GTK_GRID(tone_grid), 0, 1, "GAMMA", GTK_WIDGET(app->gamma_scale));
    append_labeled(GTK_GRID(tone_grid), 1, 1, "SATURATION", GTK_WIDGET(app->saturation_scale));
    append_labeled(GTK_GRID(tone_grid), 1, 2, "EDGE ENHANCEMENT", GTK_WIDGET(app->edge_scale));
    gtk_box_append(GTK_BOX(panel), tone_grid);

    invert = gtk_check_button_new_with_label("Invert brightness");
    app->invert_button = GTK_CHECK_BUTTON(invert);
    g_signal_connect(invert, "toggled", G_CALLBACK(on_invert_toggled), app);
    gtk_box_append(GTK_BOX(panel), invert);

    grid = GTK_WIDGET(gtk_grid_new());
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    threshold = make_dropdown(threshold_items);
    app->threshold_dropdown = GTK_DROP_DOWN(threshold);
    app->threshold_scale = GTK_SCALE(make_scale(0.0, 1.0, 0.01, 0.5, app));
    g_signal_connect(threshold, "notify::selected", G_CALLBACK(on_threshold_selected), app);
    append_labeled(GTK_GRID(grid), 0, 0, "THRESHOLD", threshold);
    append_labeled(GTK_GRID(grid), 1, 0, "THRESHOLD LEVEL", GTK_WIDGET(app->threshold_scale));
    gtk_box_append(GTK_BOX(panel), grid);

    app->cell_width = 18;
    app->cell_height = 18;
    app->glyph_aspect = 0.58f;
    app->ramp_levels = 10;
    app->color_mode = ASCII_COLOR_GRAYSCALE;
    app->brightness = 0.0f;
    app->contrast = 1.18f;
    app->gamma = 0.92f;
    app->saturation = 1.0f;
    app->edge = 0.0f;
    app->threshold = 0.5f;
    app->threshold_enabled = FALSE;
    app->invert = FALSE;
    return panel;
}

static void cleanup_export_files(AppState *app)
{
    if (app->export_temp_dir != NULL) {
        GDir *dir = g_dir_open(app->export_temp_dir, 0, NULL);
        const gchar *name;
        if (dir != NULL) {
            while ((name = g_dir_read_name(dir)) != NULL) {
                gchar *file = g_build_filename(app->export_temp_dir, name, NULL);
                g_remove(file);
                g_free(file);
            }
            g_dir_close(dir);
    }
    g_rmdir(app->export_temp_dir);
    }
    g_clear_pointer(&app->export_temp_dir, g_free);
    g_clear_pointer(&app->export_temp_pattern, g_free);
}

static void free_export_frame_task(ExportFrameTask *task)
{
    if (task == NULL) {
        return;
    }
    g_free(task->frame.pixels);
    clear_render_settings(&task->settings);
    g_free(task);
}

static void free_export_frame_result(ExportFrameResult *result)
{
    if (result == NULL) {
        return;
    }
    g_free(result->pixels);
    g_free(result);
}

static gboolean export_failure_on_main(gpointer user_data)
{
    AppState *app = user_data;
    gboolean exporting;

    g_mutex_lock(&app->export_lock);
    app->export_failure_source_id = 0;
    exporting = app->exporting;
    g_mutex_unlock(&app->export_lock);
    if (exporting) {
        stop_export(app, FALSE);
    }
    return G_SOURCE_REMOVE;
}

static void export_signal_failure(AppState *app)
{
    g_mutex_lock(&app->export_lock);
    if (!app->export_stop) {
        app->export_stop = TRUE;
        g_cond_broadcast(&app->export_cond);
        g_cond_broadcast(&app->export_space_cond);
        if (app->export_failure_source_id == 0) {
            app->export_failure_source_id = g_idle_add(export_failure_on_main, app);
        }
    }
    g_mutex_unlock(&app->export_lock);
}

static void export_render_task(gpointer user_data, gpointer pool_data)
{
    ExportFrameTask *task = user_data;
    AppState *app = pool_data;
    ExportFrameResult *result = NULL;
    guint8 *pixels = NULL;
    gint width = 0;
    gint height = 0;
    gsize size = 0;
    gboolean stopped;

    g_mutex_lock(&app->export_lock);
    stopped = app->export_stop;
    g_mutex_unlock(&app->export_lock);

    if (!stopped) {
        pixels = cpu_render_ascii(&task->frame,
                                  &task->settings,
                                  app->export_width,
                                  app->export_height,
                                  &width,
                                  &height,
                                  &size);
        if (pixels != NULL) {
            result = g_new0(ExportFrameResult, 1);
            result->frame_number = task->frame_number;
            result->pixels = pixels;
            result->size = size;
            result->width = width;
            result->height = height;
        }
    }

    g_mutex_lock(&app->export_lock);
    if (!app->export_stop && result != NULL) {
        guint64 *key = g_new(guint64, 1);
        *key = result->frame_number;
        g_hash_table_insert(app->export_results, key, result);
        result = NULL;
        g_cond_signal(&app->export_cond);
    } else {
        if (app->export_pending > 0) {
            app->export_pending--;
        }
        g_cond_signal(&app->export_space_cond);
    }
    g_mutex_unlock(&app->export_lock);

    free_export_frame_result(result);
    free_export_frame_task(task);
    if (pixels == NULL && !stopped) {
        export_signal_failure(app);
    }
}

static gpointer export_writer_main(gpointer user_data)
{
    AppState *app = user_data;

    for (;;) {
        ExportFrameResult *result = NULL;
        gboolean send_eos = FALSE;
        gboolean stopped;
        guint64 key;
        gpointer stored_key = NULL;

        g_mutex_lock(&app->export_lock);
        for (;;) {
            key = app->export_next_write_frame;
            if (g_hash_table_lookup_extended(app->export_results,
                                             &key,
                                             &stored_key,
                                             (gpointer *)&result)) {
                g_hash_table_steal(app->export_results, &key);
                g_free(stored_key);
                break;
            }
            if (app->export_stop) {
                break;
            }
            if (app->export_decode_eos && app->export_pending == 0) {
                send_eos = TRUE;
                break;
            }
            g_cond_wait(&app->export_cond, &app->export_lock);
        }
        stopped = app->export_stop;
        g_mutex_unlock(&app->export_lock);

        if (stopped) {
            free_export_frame_result(result);
            break;
        }
        if (send_eos) {
            gst_app_src_end_of_stream(GST_APP_SRC(app->export_appsrc));
            break;
        }

        {
            GstBuffer *output = gst_buffer_new_allocate(NULL, result->size, NULL);
            GstFlowReturn flow;

            if (output == NULL) {
                free_export_frame_result(result);
                export_signal_failure(app);
                break;
            }
            gst_buffer_fill(output, 0, result->pixels, result->size);
            GST_BUFFER_PTS(output) = result->frame_number * GST_SECOND / 30;
            GST_BUFFER_DURATION(output) = GST_SECOND / 30;
            flow = gst_app_src_push_buffer(GST_APP_SRC(app->export_appsrc), output);
            free_export_frame_result(result);

            g_mutex_lock(&app->export_lock);
            if (app->export_pending > 0) {
                app->export_pending--;
            }
            app->export_next_write_frame++;
            g_cond_signal(&app->export_space_cond);
            g_mutex_unlock(&app->export_lock);

            if (flow != GST_FLOW_OK) {
                export_signal_failure(app);
                break;
            }
        }
    }

    return NULL;
}

static gboolean start_export_workers(AppState *app)
{
    guint processors = g_get_num_processors();

    app->export_workers = MIN(4u, MAX(2u, processors));
    app->export_max_pending = app->export_workers + 2u;
    app->export_pending = 0;
    app->export_submitted_frame = 0;
    app->export_next_write_frame = 0;
    app->export_decode_eos = FALSE;
    app->export_stop = FALSE;
    app->export_results = g_hash_table_new_full(g_int64_hash,
                                                g_int64_equal,
                                                g_free,
                                                (GDestroyNotify)free_export_frame_result);
    app->export_pool = g_thread_pool_new(export_render_task,
                                         app,
                                         (gint)app->export_workers,
                                         FALSE,
                                         NULL);
    if (app->export_pool == NULL) {
        g_clear_pointer(&app->export_results, g_hash_table_destroy);
        return FALSE;
    }
    app->export_writer_thread = g_thread_new("ascii-export-writer",
                                             export_writer_main,
                                             app);
    if (app->export_writer_thread == NULL) {
        g_thread_pool_free(app->export_pool, FALSE, TRUE);
        app->export_pool = NULL;
        g_clear_pointer(&app->export_results, g_hash_table_destroy);
        return FALSE;
    }
    return TRUE;
}

static void stop_export(AppState *app, gboolean success)
{
    guint failure_source_id;

    g_mutex_lock(&app->export_lock);
    app->export_stop = TRUE;
    g_cond_broadcast(&app->export_cond);
    g_cond_broadcast(&app->export_space_cond);
    failure_source_id = app->export_failure_source_id;
    app->export_failure_source_id = 0;
    g_mutex_unlock(&app->export_lock);
    if (failure_source_id != 0) {
        g_source_remove(failure_source_id);
    }

    if (app->export_decode_watch_id != 0) {
        g_source_remove(app->export_decode_watch_id);
        app->export_decode_watch_id = 0;
    }
    if (app->export_output_watch_id != 0) {
        g_source_remove(app->export_output_watch_id);
        app->export_output_watch_id = 0;
    }
    if (app->export_decode_pipeline != NULL) {
        gst_element_set_state(app->export_decode_pipeline, GST_STATE_NULL);
        gst_object_unref(app->export_decode_pipeline);
        app->export_decode_pipeline = NULL;
    }
    if (app->export_output_pipeline != NULL) {
        gst_element_set_state(app->export_output_pipeline, GST_STATE_NULL);
        gst_object_unref(app->export_output_pipeline);
        app->export_output_pipeline = NULL;
    }

    if (app->export_writer_thread != NULL) {
        g_thread_join(app->export_writer_thread);
        app->export_writer_thread = NULL;
    }
    if (app->export_pool != NULL) {
        g_thread_pool_free(app->export_pool, FALSE, TRUE);
        app->export_pool = NULL;
    }
    g_mutex_lock(&app->export_lock);
    g_clear_pointer(&app->export_results, g_hash_table_destroy);
    app->export_pending = 0;
    app->export_decode_eos = FALSE;
    g_mutex_unlock(&app->export_lock);
    app->export_appsrc = NULL;
    cleanup_export_files(app);
    g_clear_pointer(&app->export_path, g_free);
    clear_render_settings(&app->export_settings);
    app->exporting = FALSE;
    if (app->export_button != NULL) {
        gtk_widget_set_sensitive(GTK_WIDGET(app->export_button), TRUE);
    }
    set_status(app, success ? "Video render finished" : "Video render failed");
}

static gboolean on_export_output_bus(GstBus *bus, GstMessage *message, gpointer user_data);

static GstElement *create_export_output(AppState *app, const gchar *path)
{
    GstElement *pipeline;
    GstElement *source;
    GstElement *convert;
    GstElement *encoder;
    GstElement *sink;
    GstElement *mux = NULL;
    GstCaps *caps;
    guint quality;
    gint quantizer;

    pipeline = gst_pipeline_new("ascii-output");
    source = gst_element_factory_make("appsrc", "export_source");
    convert = gst_element_factory_make("videoconvert", "export_convert");
    if (pipeline == NULL || source == NULL || convert == NULL) {
        g_clear_object(&pipeline);
        return NULL;
    }

    caps = gst_caps_new_simple("video/x-raw",
                               "format", G_TYPE_STRING, "RGBA",
                               "width", G_TYPE_INT, app->export_width,
                               "height", G_TYPE_INT, app->export_height,
                               "framerate", GST_TYPE_FRACTION, 30, 1,
                               NULL);
    g_object_set(source, "caps", caps, "format", GST_FORMAT_TIME, "is-live", FALSE, NULL);
    gst_caps_unref(caps);

    if (app->export_format == 0 || app->export_format == 2) {
        encoder = gst_element_factory_make("x264enc", "export_h264");
        mux = gst_element_factory_make(app->export_format == 0 ? "mp4mux" : "qtmux",
                                       app->export_format == 0 ? "export_mp4" : "export_mov");
        sink = gst_element_factory_make("filesink", "export_file");
        if (encoder == NULL || mux == NULL || sink == NULL) {
            g_clear_object(&pipeline);
            return NULL;
        }
        quality = gtk_drop_down_get_selected(app->quality_dropdown);
        quantizer = quality == 0 ? 0 : quality == 1 ? 18 : 30;
        g_object_set(encoder,
                     "pass", 4,
                     "quantizer", (guint)quantizer,
                     "qp-min", (guint)quantizer,
                     "qp-max", (guint)quantizer,
                     NULL);
        g_object_set(sink, "location", path, NULL);
        gst_bin_add_many(GST_BIN(pipeline), source, convert, encoder, mux, sink, NULL);
        if (!gst_element_link_many(source, convert, encoder, mux, sink, NULL)) {
            gst_object_unref(pipeline);
            return NULL;
        }
    } else {
        encoder = gst_element_factory_make("pngenc", "export_image_frames");
        sink = gst_element_factory_make("multifilesink", "export_image_files");
        if (encoder == NULL || sink == NULL) {
            g_clear_object(&pipeline);
            return NULL;
        }
        g_object_set(sink, "location", path, NULL);
        gst_bin_add_many(GST_BIN(pipeline), source, convert, encoder, sink, NULL);
        if (!gst_element_link_many(source, convert, encoder, sink, NULL)) {
            gst_object_unref(pipeline);
            return NULL;
        }
    }

    app->export_appsrc = source;
    return pipeline;
}

static GstFlowReturn on_export_sample(GstAppSink *sink, gpointer user_data)
{
    AppState *app = user_data;
    GstSample *sample = gst_app_sink_pull_sample(sink);
    GstCaps *caps;
    GstVideoInfo info;
    GstBuffer *input;
    GstMapInfo map;
    ExportFrameTask *task;
    GError *error = NULL;
    gboolean stopped;

    if (sample == NULL) {
        return GST_FLOW_ERROR;
    }
    caps = gst_sample_get_caps(sample);
    input = gst_sample_get_buffer(sample);
    if (caps == NULL || input == NULL || !gst_video_info_from_caps(&info, caps) ||
        !gst_buffer_map(input, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    task = g_new0(ExportFrameTask, 1);
    task->app = app;
    task->frame.size = map.size;
    task->frame.pixels = g_malloc(map.size);
    memcpy(task->frame.pixels, map.data, map.size);
    task->frame.width = GST_VIDEO_INFO_WIDTH(&info);
    task->frame.height = GST_VIDEO_INFO_HEIGHT(&info);
    task->frame.stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
    copy_render_settings(&task->settings, &app->export_settings);
    gst_buffer_unmap(input, &map);
    gst_sample_unref(sample);

    g_mutex_lock(&app->export_lock);
    while (!app->export_stop && app->export_pending >= app->export_max_pending) {
        g_cond_wait(&app->export_space_cond, &app->export_lock);
    }
    stopped = app->export_stop;
    if (!stopped) {
        task->frame_number = app->export_submitted_frame++;
        app->export_pending++;
        if (g_thread_pool_push(app->export_pool, task, &error)) {
            g_mutex_unlock(&app->export_lock);
            return GST_FLOW_OK;
        }
        g_clear_error(&error);
        if (app->export_pending > 0) {
            app->export_pending--;
        }
        g_cond_signal(&app->export_space_cond);
    }
    g_mutex_unlock(&app->export_lock);

    if (stopped) {
        free_export_frame_task(task);
        return GST_FLOW_FLUSHING;
    }
    if (task != NULL) {
        free_export_frame_task(task);
        export_signal_failure(app);
    }
    return GST_FLOW_ERROR;
}

static gboolean on_export_decode_bus(GstBus *bus, GstMessage *message, gpointer user_data)
{
    AppState *app = user_data;

    (void)bus;
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
        stop_export(app, FALSE);
    } else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS) {
        g_mutex_lock(&app->export_lock);
        app->export_decode_eos = TRUE;
        g_cond_signal(&app->export_cond);
        g_mutex_unlock(&app->export_lock);
    }
    return G_SOURCE_CONTINUE;
}

static gboolean on_export_output_bus(GstBus *bus, GstMessage *message, gpointer user_data)
{
    AppState *app = user_data;
    GError *error = NULL;
    gchar *debug = NULL;

    (void)bus;
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
        gst_message_parse_error(message, &error, &debug);
        g_warning("Export error: %s (%s)",
                  error != NULL ? error->message : "unknown",
                  debug != NULL ? debug : "no details");
        g_clear_error(&error);
        g_free(debug);
        stop_export(app, FALSE);
    } else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS) {
        if (app->export_format == 1 || app->export_format == 3) {
            gchar *ffmpeg;
#ifdef ASCII_VIDEO_FFMPEG_PATH
            ffmpeg = g_strdup(ASCII_VIDEO_FFMPEG_PATH);
#else
            ffmpeg = g_find_program_in_path("ffmpeg");
#endif
            if (ffmpeg == NULL) {
                stop_export(app, FALSE);
            } else {
                GError *run_error = NULL;
                GSubprocess *process;
                if (app->export_format == 1) {
                    process = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                               G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                                               &run_error,
                                               ffmpeg, "-y", "-framerate", "30",
                                               "-i", app->export_temp_pattern,
                                               "-plays", "0", "-f", "apng",
                                               app->export_path, NULL);
                } else {
                    process = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                               G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                                               &run_error,
                                               ffmpeg, "-y", "-framerate", "30",
                                               "-i", app->export_temp_pattern,
                                               "-loop", "0", app->export_path, NULL);
                }
                if (process != NULL) {
                    gboolean ok = g_subprocess_wait_check(process, NULL, &run_error);
                    g_object_unref(process);
                    stop_export(app, ok);
                } else {
                    stop_export(app, FALSE);
                }
                g_clear_error(&run_error);
                g_free(ffmpeg);
            }
        } else {
            stop_export(app, TRUE);
        }
    }
    return G_SOURCE_CONTINUE;
}

static void start_export(AppState *app, const gchar *path)
{
    GstElement *decode;
    GstElement *sink;
    GstCaps *caps;
    GstBus *bus;
    gchar *output_path = NULL;
    gchar *uri;
    GError *error = NULL;
    guint resolution;

    if (app->exporting) {
        return;
    }
    if (app->video_path == NULL || app->texture_width <= 0 || app->texture_height <= 0) {
        set_status(app, "Load a video and wait for its first frame");
        return;
    }

    app->export_format = gtk_drop_down_get_selected(app->format_dropdown);
    resolution = gtk_drop_down_get_selected(app->resolution_dropdown);
    if (resolution == 2 &&
        app->preview_width > 0 && app->preview_height > 0) {
        app->export_width = app->preview_width;
        app->export_height = app->preview_height;
    } else if (resolution == 1 &&
               app->custom_width_spin != NULL &&
               app->custom_height_spin != NULL) {
        app->export_width = MAX(2, (gint)gtk_spin_button_get_value(app->custom_width_spin));
        app->export_height = MAX(2, (gint)gtk_spin_button_get_value(app->custom_height_spin));
    } else {
        app->export_width = app->texture_width;
        app->export_height = app->texture_height;
    }
    if (resolution == 3) {
        app->export_width = MAX(2, app->texture_width / 2);
        app->export_height = MAX(2, app->texture_height / 2);
    }
    read_render_settings(app, &app->export_settings);
    if (app->preview_width > 0 && app->preview_height > 0 &&
        (app->export_width != app->preview_width ||
         app->export_height != app->preview_height)) {
        app->export_settings.cell_width = MAX(1,
            (gint)round((gdouble)app->export_settings.cell_width *
                        app->export_width / app->preview_width));
        app->export_settings.cell_height = MAX(1,
            (gint)round((gdouble)app->export_settings.cell_height *
                        app->export_height / app->preview_height));
    }
    app->export_path = g_strdup(path);

    if (app->export_format == 0 || app->export_format == 2) {
        output_path = g_strdup(path);
        const gchar *extension = app->export_format == 0 ? ".mp4" : ".mov";
        if (!g_str_has_suffix(output_path, extension)) {
            gchar *with_extension = g_strconcat(output_path, extension, NULL);
            g_free(output_path);
            output_path = with_extension;
        }
    } else {
        app->export_temp_dir = g_dir_make_tmp("ascii-video-XXXXXX", &error);
        if (app->export_temp_dir == NULL) {
            set_status(app, error->message);
            g_clear_error(&error);
            g_clear_pointer(&app->export_path, g_free);
            clear_render_settings(&app->export_settings);
            return;
        }
        app->export_temp_pattern = g_build_filename(app->export_temp_dir, "frame-%05d.png", NULL);
        output_path = g_strdup(app->export_temp_pattern);
        if (app->export_format == 1 && !g_str_has_suffix(app->export_path, ".png")) {
            gchar *with_extension = g_strconcat(app->export_path, ".png", NULL);
            g_free(app->export_path);
            app->export_path = with_extension;
        } else if (app->export_format == 3 && !g_str_has_suffix(app->export_path, ".gif")) {
            gchar *with_extension = g_strconcat(app->export_path, ".gif", NULL);
            g_free(app->export_path);
            app->export_path = with_extension;
        }
    }

    app->export_output_pipeline = create_export_output(app, output_path);
    g_free(output_path);
    if (app->export_output_pipeline == NULL) {
        stop_export(app, FALSE);
        set_status(app, "Required video encoder is unavailable");
        return;
    }
    bus = gst_element_get_bus(app->export_output_pipeline);
    app->export_output_watch_id = gst_bus_add_watch(bus, on_export_output_bus, app);
    gst_object_unref(bus);
    app->exporting = TRUE;
    gtk_widget_set_sensitive(GTK_WIDGET(app->export_button), FALSE);
    if (!start_export_workers(app)) {
        stop_export(app, FALSE);
        set_status(app, "Could not start export workers");
        return;
    }

    decode = gst_element_factory_make("playbin", "ascii_export_decode");
    sink = gst_element_factory_make("appsink", "ascii_export_sink");
    if (decode == NULL || sink == NULL) {
        stop_export(app, FALSE);
        return;
    }
    caps = gst_caps_from_string("video/x-raw,format=RGBA");
    gst_app_sink_set_caps(GST_APP_SINK(sink), caps);
    gst_caps_unref(caps);
    g_object_set(sink, "emit-signals", TRUE, "sync", FALSE,
                 "max-buffers", 2u, "drop", FALSE, "enable-last-sample", FALSE, NULL);
    g_signal_connect(sink, "new-sample", G_CALLBACK(on_export_sample), app);
    uri = gst_filename_to_uri(app->video_path, NULL);
    g_object_set(decode, "uri", uri, "video-sink", sink, NULL);
    gst_object_unref(sink);
    g_free(uri);
    app->export_decode_pipeline = decode;
    bus = gst_element_get_bus(decode);
    app->export_decode_watch_id = gst_bus_add_watch(bus, on_export_decode_bus, app);
    gst_object_unref(bus);
    set_status(app, "Rendering video…");
    gst_element_set_state(app->export_output_pipeline, GST_STATE_PLAYING);
    gst_element_set_state(app->export_decode_pipeline, GST_STATE_PLAYING);
}

static void on_export_dialog_response(GtkNativeDialog *dialog,
                                      gint response,
                                      gpointer user_data)
{
    AppState *app = user_data;

    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
        if (file != NULL) {
            gchar *path = g_file_get_path(file);
            if (path != NULL) {
                start_export(app, path);
                g_free(path);
            }
            g_object_unref(file);
        }
    }
    gtk_native_dialog_destroy(dialog);
}

static void on_export_clicked(GtkButton *button, gpointer user_data)
{
    AppState *app = user_data;
    GtkFileChooserNative *dialog;
    GtkFileFilter *filter;
    const gchar *name;

    (void)button;
    if (gtk_drop_down_get_selected(app->format_dropdown) == 0) {
        name = "ascii-render.mp4";
    } else if (gtk_drop_down_get_selected(app->format_dropdown) == 1) {
        name = "ascii-render.png";
    } else if (gtk_drop_down_get_selected(app->format_dropdown) == 2) {
        name = "ascii-render.mov";
    } else {
        name = "ascii-render.gif";
    }
    dialog = gtk_file_chooser_native_new("Render video",
                                         GTK_WINDOW(app->window),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         "Render",
                                         "Cancel");
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), name);
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Video output");
    gtk_file_filter_add_pattern(filter, "*.png");
    gtk_file_filter_add_pattern(filter, "*.mp4");
    gtk_file_filter_add_pattern(filter, "*.mov");
    gtk_file_filter_add_pattern(filter, "*.gif");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    g_signal_connect(dialog, "response", G_CALLBACK(on_export_dialog_response), app);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));
}

static GtkWidget *build_export_panel(AppState *app)
{
    static const gchar *format_items[] = {"MP4 video", "PNG animation", "MOV video", "GIF animation", NULL};
    static const gchar *quality_items[] = {"Lossless", "High", "Draft", NULL};
    static const gchar *resolution_items[] = {"Native dimensions", "Custom resolution", "Preview dimensions", "Half native", NULL};
    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *custom_grid;
    GtkWidget *button;

    gtk_widget_set_margin_start(panel, 10);
    gtk_widget_set_margin_end(panel, 22);
    gtk_widget_set_margin_bottom(panel, 22);
    gtk_box_append(GTK_BOX(panel), section_label("RENDER VIDEO"));
    gtk_box_append(GTK_BOX(panel), field_label("FORMAT"));
    app->format_dropdown = GTK_DROP_DOWN(make_dropdown(format_items));
    gtk_drop_down_set_selected(app->format_dropdown, 0);
    gtk_box_append(GTK_BOX(panel), GTK_WIDGET(app->format_dropdown));
    gtk_box_append(GTK_BOX(panel), field_label("QUALITY"));
    app->quality_dropdown = GTK_DROP_DOWN(make_dropdown(quality_items));
    gtk_box_append(GTK_BOX(panel), GTK_WIDGET(app->quality_dropdown));
    gtk_box_append(GTK_BOX(panel), field_label("RESOLUTION"));
    app->resolution_dropdown = GTK_DROP_DOWN(make_dropdown(resolution_items));
    gtk_drop_down_set_selected(app->resolution_dropdown, 0);
    g_signal_connect(app->resolution_dropdown,
                     "notify::selected",
                     G_CALLBACK(on_export_resolution_selected),
                     app);
    gtk_box_append(GTK_BOX(panel), GTK_WIDGET(app->resolution_dropdown));

    custom_grid = GTK_WIDGET(gtk_grid_new());
    gtk_grid_set_column_spacing(GTK_GRID(custom_grid), 10);
    app->custom_width_spin = GTK_SPIN_BUTTON(make_spin(2, 16384, 960, app));
    app->custom_height_spin = GTK_SPIN_BUTTON(make_spin(2, 16384, 640, app));
    g_signal_connect(app->custom_width_spin,
                     "value-changed",
                     G_CALLBACK(on_custom_resolution_changed),
                     app);
    g_signal_connect(app->custom_height_spin,
                     "value-changed",
                     G_CALLBACK(on_custom_resolution_changed),
                     app);
    append_labeled(GTK_GRID(custom_grid), 0, 0, "WIDTH (PX)", GTK_WIDGET(app->custom_width_spin));
    append_labeled(GTK_GRID(custom_grid), 1, 0, "HEIGHT (PX)", GTK_WIDGET(app->custom_height_spin));
    app->custom_resolution_grid = custom_grid;
    gtk_widget_set_visible(custom_grid, FALSE);
    gtk_box_append(GTK_BOX(panel), custom_grid);

    button = gtk_button_new_with_label("Render video");
    gtk_widget_add_css_class(button, "suggested-action");
    gtk_widget_set_margin_top(button, 10);
    g_signal_connect(button, "clicked", G_CALLBACK(on_export_clicked), app);
    gtk_box_append(GTK_BOX(panel), button);
    app->export_button = GTK_BUTTON(button);
    return panel;
}

static GtkWidget *make_preview_card(AppState *app, gboolean source)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *titles = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *overline = gtk_label_new(source ? "SOURCE" : "RENDERED");
    GtkWidget *title = gtk_label_new(source ? "Original signal" : "ASCII signal");
    GtkWidget *badge = source ? gtk_label_new("") : NULL;
    GtkWidget *overlay = gtk_overlay_new();
    GtkWidget *gl_area = NULL;
    GtkWidget *picture = gtk_picture_new();
    GtkWidget *empty;

    gtk_widget_set_hexpand(card, TRUE);
    gtk_widget_set_vexpand(card, TRUE);
    gtk_widget_set_size_request(card, 320, 380);
    gtk_widget_set_hexpand(header, TRUE);
    gtk_widget_set_margin_start(header, 4);
    gtk_widget_set_margin_end(header, 4);
    gtk_widget_set_hexpand(titles, TRUE);
    gtk_widget_set_halign(titles, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(titles), overline);
    gtk_box_append(GTK_BOX(titles), title);
    gtk_box_append(GTK_BOX(header), titles);
    if (badge != NULL) {
        gtk_widget_set_halign(badge, GTK_ALIGN_END);
        gtk_widget_set_valign(badge, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(header), badge);
    }

    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_hexpand(picture, TRUE);
    gtk_widget_set_vexpand(picture, TRUE);
    if (source) {
        app->source_meta_label = GTK_LABEL(badge);
    }
    if (app->cpu_fallback) {
        gtk_widget_set_visible(picture, TRUE);
        gtk_overlay_set_child(GTK_OVERLAY(overlay), picture);
    } else {
        gl_area = gtk_gl_area_new();
        gtk_gl_area_set_required_version(GTK_GL_AREA(gl_area), 3, 3);
        gtk_gl_area_set_auto_render(GTK_GL_AREA(gl_area), FALSE);
        gtk_widget_set_hexpand(gl_area, TRUE);
        gtk_widget_set_vexpand(gl_area, TRUE);
        gtk_widget_set_size_request(gl_area, 320, 380);
        if (source) {
            app->source_gl_area = GTK_GL_AREA(gl_area);
            g_signal_connect(gl_area, "realize", G_CALLBACK(on_source_gl_realize), app);
            g_signal_connect(gl_area, "render", G_CALLBACK(on_source_gl_render), app);
            g_signal_connect(gl_area, "unrealize", G_CALLBACK(on_source_gl_unrealize), app);
        } else {
            app->gl_area = GTK_GL_AREA(gl_area);
            g_signal_connect(gl_area, "realize", G_CALLBACK(on_gl_realize), app);
            g_signal_connect(gl_area, "render", G_CALLBACK(on_gl_render), app);
            g_signal_connect(gl_area, "unrealize", G_CALLBACK(on_gl_unrealize), app);
        }
        gtk_widget_set_visible(gl_area, FALSE);
        gtk_overlay_set_child(GTK_OVERLAY(overlay), gl_area);
        gtk_widget_set_visible(picture, FALSE);
        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), picture);
        if (!source) {
            g_signal_connect(overlay, "map", G_CALLBACK(on_render_overlay_map), gl_area);
        }
    }
    if (!app->cpu_fallback) {
        gtk_overlay_set_measure_overlay(GTK_OVERLAY(overlay), picture, TRUE);
    }
    if (source) {
        app->source_picture = GTK_PICTURE(picture);
    } else {
        app->ascii_picture = GTK_PICTURE(picture);
    }

    empty = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_halign(empty, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(empty, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(empty), gtk_label_new(source ? "Choose a source video" : "ASCII output will appear here"));
    if (source) {
        GtkWidget *button = gtk_button_new_with_label("Choose video");
        gtk_widget_add_css_class(button, "suggested-action");
        g_signal_connect(button, "clicked", G_CALLBACK(on_open_clicked), app);
        gtk_box_append(GTK_BOX(empty), button);
        app->source_empty_state = empty;
    } else {
        app->empty_state = empty;
    }
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), empty);
    gtk_overlay_set_measure_overlay(GTK_OVERLAY(overlay), empty, FALSE);
    gtk_box_append(GTK_BOX(card), header);
    gtk_box_append(GTK_BOX(card), overlay);
    return card;
}

static void build_window(AppState *app)
{
    GtkWidget *root;
    GtkWidget *scroll;
    GtkWidget *preview_row;
    GtkWidget *lower_row;
    GtkWidget *settings_scroll;
    GtkWidget *settings;
    GtkWidget *export_panel;
    GtkWidget *playback;
    GtkWidget *open_button;
    GtkWidget *header;
    GtkWidget *status;

    app->window = GTK_APPLICATION_WINDOW(gtk_application_window_new(app->application));
    gtk_window_set_title(GTK_WINDOW(app->window), "ASCII Video");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1440, 920);

    header = gtk_header_bar_new();
    open_button = gtk_button_new_from_icon_name("document-open-symbolic");
    gtk_widget_set_tooltip_text(open_button, "Open video");
    g_signal_connect(open_button, "clicked", G_CALLBACK(on_open_clicked), app);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), open_button);

    app->title_label = GTK_LABEL(gtk_label_new("ASCII Video"));
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), GTK_WIDGET(app->title_label));
    gtk_window_set_titlebar(GTK_WINDOW(app->window), header);

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), root);

    preview_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_margin_top(preview_row, 22);
    gtk_widget_set_margin_start(preview_row, 22);
    gtk_widget_set_margin_end(preview_row, 22);
    gtk_widget_set_size_request(preview_row, -1, 410);
    gtk_box_append(GTK_BOX(preview_row), make_preview_card(app, TRUE));
    gtk_box_append(GTK_BOX(preview_row), make_preview_card(app, FALSE));
    gtk_box_append(GTK_BOX(root), preview_row);

    playback = build_playback_bar(app);
    gtk_box_append(GTK_BOX(root), playback);

    lower_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    settings = build_settings_panel(app);
    settings_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(settings_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(settings_scroll, TRUE);
    gtk_widget_set_vexpand(settings_scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(settings_scroll), settings);
    export_panel = build_export_panel(app);
    gtk_widget_set_size_request(export_panel, 320, -1);
    gtk_widget_set_hexpand(export_panel, FALSE);
    gtk_widget_set_halign(export_panel, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(lower_row), settings_scroll);
    gtk_box_append(GTK_BOX(lower_row), export_panel);
    gtk_box_append(GTK_BOX(root), lower_row);

    status = gtk_label_new("Ready");
    gtk_widget_set_halign(status, GTK_ALIGN_START);
    gtk_widget_set_margin_start(status, 22);
    gtk_widget_set_margin_bottom(status, 12);
    app->status_label = GTK_LABEL(status);
    gtk_box_append(GTK_BOX(root), status);
    gtk_window_set_child(GTK_WINDOW(app->window), scroll);
    g_signal_connect(app->window, "close-request", G_CALLBACK(on_window_close_request), app);
    start_render_worker(app);
    gtk_window_present(GTK_WINDOW(app->window));
}

static void on_activate(GtkApplication *application, gpointer user_data)
{
    AppState *app = user_data;

    if (app->window == NULL) {
        app->application = application;
        build_window(app);
    } else {
        gtk_window_present(GTK_WINDOW(app->window));
    }
}

int main(int argc, char **argv)
{
    GtkApplication *application;
    AppState *app;
    int status;

    gst_init(&argc, &argv);

    app = g_new0(AppState, 1);
    g_mutex_init(&app->frame_lock);
    g_mutex_init(&app->render_lock);
    g_cond_init(&app->render_cond);
    g_mutex_init(&app->export_lock);
    g_cond_init(&app->export_cond);
    g_cond_init(&app->export_space_cond);
    app->duration = GST_CLOCK_TIME_NONE;
    app->playback_rate = 1.0;
    app->cpu_fallback = TRUE;

    /* The application is CPU-rendered, so use GTK's software renderer. This
     * is scoped to this process and leaves the system GTK theme untouched. */
    g_setenv("GSK_RENDERER", "cairo", TRUE);

    application = gtk_application_new("com.example.AsciiVideo",
                                      G_APPLICATION_HANDLES_OPEN);
    g_signal_connect(application, "activate", G_CALLBACK(on_activate), app);
    g_signal_connect(application, "open", G_CALLBACK(on_open_files), app);

    status = g_application_run(G_APPLICATION(application), argc, argv);

    stop_pipeline(app);
    stop_render_worker(app);
    g_mutex_clear(&app->frame_lock);
    g_mutex_clear(&app->render_lock);
    g_cond_clear(&app->render_cond);
    g_mutex_clear(&app->export_lock);
    g_cond_clear(&app->export_cond);
    g_cond_clear(&app->export_space_cond);
    g_free(app);
    g_object_unref(application);
    return status;
}
