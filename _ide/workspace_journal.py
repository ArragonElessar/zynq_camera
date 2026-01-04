# 2026-01-04T14:17:21.611492
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

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

