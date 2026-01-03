# 2026-01-03T21:25:22.113778
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

vitis.dispose()

