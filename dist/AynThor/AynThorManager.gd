extends Node

@export_group("Screens")
@export var main_screen: Screen
@export var second_screen: Screen

@export var target_fps: int = 0:
	set(value):
		target_fps = value
		if renderer:
			renderer.set_target_fps(value)

@export var rotation_degrees: ScreenRotation = ScreenRotation.DEG_180:
	set(value):
		rotation_degrees = value
		if renderer:
			renderer.set_rotation_degrees(value)
			
var _main_viewport: SubViewport
var _second_viewport: SubViewport
var _main_container: SubViewportContainer
var _second_container: SubViewportContainer

var ayn_thor_plugin = null
var renderer = null

var is_swapped: bool = false
var original_main_size: Vector2i
var original_second_size: Vector2i
var is_android: bool = false
var second_size_confirmed: bool = false
var last_touch_positions: Dictionary = {}

signal screens_swapped(swapped: bool)

func _input(event: InputEvent) -> void:
	if event.is_action_pressed("swap"):
		swap_screens()
		
func _ready():
	is_android = OS.get_name() == "Android"
	
	if Engine.has_singleton("AynThor"):
		if main_screen:
			main_screen.setup(self)
			if main_screen.viewport: _main_viewport = main_screen.viewport
			if main_screen.container: _main_container = main_screen.container
			
		if second_screen:
			second_screen.setup(self)
			if second_screen.container: _second_container = second_screen.container
			if second_screen.viewport: _second_viewport = second_screen.viewport
		
		ayn_thor_plugin = Engine.get_singleton("AynThor")
		ayn_thor_plugin.connect("second_screen_connected", _on_screen_connected)
		ayn_thor_plugin.connect("second_screen_input", _on_second_screen_input)
		ayn_thor_plugin.init_screen()
	
	if ClassDB.class_exists("AynThorRenderer"):
		renderer = get_node_or_null("AynThorRenderer")
		if not renderer:
			renderer = ClassDB.instantiate("AynThorRenderer")
			add_child(renderer)
			renderer.name = "AynThorRenderer"
		renderer.set_target_fps(target_fps)
		renderer.set_rotation_degrees(rotation_degrees)
	
	original_main_size = get_viewport().size
	if original_main_size.x == 0:
		original_main_size = Vector2i(1920, 1080)
	
	original_second_size = Vector2i(854, 480)
	
	if _main_viewport:
		_main_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		_main_viewport.size = original_main_size
	if _second_viewport:
		_second_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		_second_viewport.size = original_second_size

	if _main_container:
		_main_container.position = Vector2.ZERO
	if _second_container:
		_second_container.position = Vector2(-10000, 0)

var skip_frames: int = 0

func _process(_delta):
	if skip_frames > 0:
		skip_frames -= 1
		return

	if not renderer or not renderer.is_window_available():
		return
	
	if not second_size_confirmed:
		_try_update_second_screen_size()

	var texture_to_draw = _main_viewport if is_swapped else _second_viewport
	if texture_to_draw:
		renderer.draw_viewport_texture(texture_to_draw.get_texture().get_rid())

func _try_update_second_screen_size():
	if renderer and renderer.is_window_available():
		var size = renderer.get_second_screen_size()
		if size != Vector2i.ZERO:
			original_second_size = size
			second_size_confirmed = true
			if not is_swapped and _second_viewport:
				_second_viewport.size = original_second_size

func swap_screens():
	is_swapped = !is_swapped
	
	var second_size = original_second_size
	if renderer and renderer.is_window_available():
		var s = renderer.get_second_screen_size()
		if s != Vector2i.ZERO:
			second_size = s
			original_second_size = s

	if is_swapped:
		if _main_container:
			_main_container.position = Vector2(-10000, 0)
			_main_container.size = second_size
		if _second_container:
			_second_container.position = Vector2.ZERO
			_second_container.size = original_main_size
		
		if _second_viewport:
			_second_viewport.size = original_main_size
		if _main_viewport:
			_main_viewport.size = second_size
	else:
		if _main_container:
			_main_container.position = Vector2.ZERO
			_main_container.size = original_main_size
		if _second_container:
			_second_container.position = Vector2(-10000, 0)
			_second_container.size = second_size
		
		if _main_viewport:
			_main_viewport.size = original_main_size
		if _second_viewport:
			_second_viewport.size = second_size

	screens_swapped.emit(is_swapped)
	skip_frames = 2

func _on_second_screen_input(action: int, x: float, y: float, pid: int):
	var event = null
	var current_pos = Vector2(x, y)
	
	if action == 0 or action == 5:
		last_touch_positions[pid] = current_pos
		var touch = InputEventScreenTouch.new()
		touch.pressed = true
		touch.index = pid
		touch.position = current_pos
		event = touch
	elif action == 1 or action == 6:
		last_touch_positions.erase(pid)
		var touch = InputEventScreenTouch.new()
		touch.pressed = false
		touch.index = pid
		touch.position = current_pos
		event = touch
	elif action == 2:
		var drag = InputEventScreenDrag.new()
		drag.index = pid
		drag.position = current_pos
		drag.relative = current_pos - last_touch_positions.get(pid, current_pos)
		last_touch_positions[pid] = current_pos
		event = drag
		
	if event:
		var target_vp = _main_viewport if is_swapped else _second_viewport
		if target_vp:
			target_vp.push_input(event)

func _on_screen_connected():
	_try_update_second_screen_size()

func _notification(what):
	if what == NOTIFICATION_WM_CLOSE_REQUEST:
		if ayn_thor_plugin:
			ayn_thor_plugin.destroy_screen()

enum ScreenRotation {
	DEG_0 = 0,
	DEG_90 = 90,
	DEG_180 = 180,
	DEG_270 = 270
}