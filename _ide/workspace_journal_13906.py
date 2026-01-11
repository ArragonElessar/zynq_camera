# 2026-01-08T19:30:17.027774
import vitis

client = vitis.create_client()
client.set_workspace(path="application")

platform = client.get_component(name="camera_platform")
status = platform.build()

comp = client.get_component(name="camera_application")
comp.build()

