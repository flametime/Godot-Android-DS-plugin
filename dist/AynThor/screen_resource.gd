class_name Screen extends Resource

@export var container_path: NodePath
@export var viewport_path: NodePath
	
var container: SubViewportContainer
var viewport: SubViewport
	
func setup(base_node: Node):
	if container_path: container = base_node.get_node(container_path)
	if viewport_path: viewport = base_node.get_node(viewport_path)
