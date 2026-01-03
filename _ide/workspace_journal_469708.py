# 2026-01-03T19:09:00.483896
import vitis

client = vitis.create_client()
client.set_workspace(path="application")

advanced_options = client.create_advanced_options_dict(dt_overlay="0")

platform = client.create_platform_component(name = "camera_platform",hw_design = "$COMPONENT_LOCATION/../../camera_hardware.xsa",os = "standalone",cpu = "ps7_cortexa9_0",domain_name = "standalone_ps7_cortexa9_0",generate_dtb = False,advanced_options = advanced_options,compiler = "gcc")

comp = client.create_app_component(name="camera_application",platform = "$COMPONENT_LOCATION/../camera_platform/export/camera_platform/camera_platform.xpfm",domain = "standalone_ps7_cortexa9_0",template = "hello_world")

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

vitis.dispose()

