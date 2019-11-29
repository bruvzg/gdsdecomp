extends Node

func test():
	yield(get_tree(), "idle_frame")
	return 100
