//! What's particular to this device.
pub const SELFTEST_VAR: &str = "PADLAB_GPUI_SELFTEST";
pub const WINDOW: (f32, f32) = (1500.0 * 0.8, 940.0 * 0.8);
pub const WINDOW_COLOUR: u32 = 0x101012;
/// (the self-test) A field that opens a menu when clicked, in panel pixels.
pub const MENU_AT: Option<(f32, f32)> = Some((202.0, 136.0));
