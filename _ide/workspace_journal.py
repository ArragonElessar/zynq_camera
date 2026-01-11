# 2026-01-11T14:02:58.649602
import vitis

client = vitis.create_client()
client.set_workspace(path="application")

platform = client.get_component(name="camera_platform")
status = platform.build()

comp = client.get_component(name="camera_application")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

