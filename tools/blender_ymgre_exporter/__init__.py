bl_info = {
    "name": "YMGRE Scene Exporter",
    "author": "YMGRE",
    "version": (1, 0, 0),
    "blender": (3, 6, 0),
    "location": "File > Export > YMGRE Scene (.scene)",
    "description": "Export cameras, lights, transforms and Ogre mesh references for YMGRE",
    "category": "Import-Export",
}

import math
import os
import re
import xml.etree.ElementTree as ET

import bpy
from bpy_extras.io_utils import ExportHelper
from bpy.props import BoolProperty, StringProperty
from mathutils import Vector


NS = "https://ymgre.dev/schema/scene/1"
ET.register_namespace("ymgre", NS)


def ns(name):
    return "{%s}%s" % (NS, name)


def safe_name(name):
    value = re.sub(r"[^A-Za-z0-9_.-]+", "_", name).strip("_")
    return value or "Object"


def position(vector):
    return vector.x, vector.z, -vector.y


def color_attributes(color):
    return {"r": "%.6g" % color[0], "g": "%.6g" % color[1], "b": "%.6g" % color[2]}


def add_transform(node, obj):
    x, y, z = position(obj.matrix_world.translation)
    ET.SubElement(node, "position", x="%.9g" % x, y="%.9g" % y, z="%.9g" % z)
    angle = -obj.matrix_world.to_euler("XYZ").z
    ET.SubElement(node, "rotation", qx="0", qy="%.9g" % math.sin(angle / 2.0),
                  qz="0", qw="%.9g" % math.cos(angle / 2.0))
    scale = max(0.001, sum(abs(value) for value in obj.scale) / 3.0)
    ET.SubElement(node, "scale", x="%.9g" % scale, y="%.9g" % scale, z="%.9g" % scale)


def add_editor_data(node, obj):
    data = ET.SubElement(node, "userData")
    values = {
        "fixed": bool(obj.get("ymgre_fixed", False)),
        "visible": not obj.hide_render,
        "wireframe": bool(obj.get("ymgre_wireframe", False)),
    }
    for name, value in values.items():
        ET.SubElement(data, "property", name=name, type="bool", data="true" if value else "false")


def object_target(obj, distance=10.0):
    origin = obj.matrix_world.translation
    forward = obj.matrix_world.to_quaternion() @ Vector((0.0, 0.0, -distance))
    return position(origin + forward)


def relative_mesh_path(obj, scene_directory):
    configured = str(obj.get("ymgre_mesh_file", "")).strip()
    if configured.startswith("//"):
        configured = bpy.path.abspath(configured)
    path = configured or (safe_name(obj.name) + ".mesh")
    if os.path.isabs(path):
        try:
            path = os.path.relpath(path, scene_directory)
        except ValueError:
            pass
    return path.replace(os.sep, "/")


def validate_dependencies(scene_directory, mesh_path):
    absolute = mesh_path if os.path.isabs(mesh_path) else os.path.join(scene_directory, mesh_path)
    errors = []
    if not os.path.isfile(absolute):
        return ["mesh missing: %s" % mesh_path]
    material = os.path.splitext(absolute)[0] + ".material"
    if not os.path.isfile(material):
        return ["material missing: %s" % os.path.basename(material)]
    with open(material, "r", encoding="utf-8", errors="ignore") as stream:
        for line in stream:
            match = re.match(r"\s*texture\s+([^\s]+)", line)
            if match and not os.path.isfile(os.path.join(os.path.dirname(material), match.group(1))):
                errors.append("texture missing: %s" % match.group(1))
    return errors


class ExportYMGREScene(bpy.types.Operator, ExportHelper):
    bl_idname = "export_scene.ymgre"
    bl_label = "Export YMGRE Scene"
    filename_ext = ".scene"

    filter_glob: StringProperty(default="*.scene", options={"HIDDEN"})
    selected_only: BoolProperty(name="Selected objects only", default=False)
    validate_resources: BoolProperty(name="Validate Ogre resources", default=True)

    def execute(self, context):
        scene = context.scene
        objects = list(context.selected_objects if self.selected_only else scene.objects)
        if scene.camera is not None and scene.camera not in objects:
            objects.append(scene.camera)
        cameras = [obj for obj in objects if obj.type == "CAMERA"]
        virtual_camera = not cameras
        node_objects = [obj for obj in objects if obj.type in {"MESH", "LIGHT", "CAMERA"}]
        scene_directory = os.path.dirname(os.path.abspath(self.filepath))

        root = ET.Element("scene", {
            "formatVersion": "1.0",
            "generator": "YMGRE Blender Exporter",
            ns("formatVersion"): "1",
        })
        environment = ET.SubElement(root, "environment")
        world_color = scene.world.color if scene.world is not None else (0.05, 0.05, 0.05)
        ET.SubElement(environment, "colourAmbient", **color_attributes(world_color))
        environment.set(ns("globalLightName"), "Global Light")
        environment.set(ns("ambientStrength"), "%.6g" % float(scene.get("ymgre_ambient_strength", 0.7)))
        nodes = ET.SubElement(root, "nodes")

        exported = []
        warnings = []
        next_index = 1
        for obj in node_objects:
            node_id = "ymgre-node-%d" % next_index
            next_index += 1
            node = ET.SubElement(nodes, "node", name=obj.name, id=node_id)
            add_transform(node, obj)
            add_editor_data(node, obj)
            if obj.type == "MESH":
                entity = ET.SubElement(node, "entity", name=obj.name)
                primitive = str(obj.get("ymgre_primitive", "")).lower()
                if primitive in {"plane", "cube", "box", "sphere", "cylinder", "cone", "torus", "capsule"}:
                    entity.set(ns("primitive"), primitive)
                else:
                    mesh_path = relative_mesh_path(obj, scene_directory)
                    entity.set("meshFile", mesh_path)
                    if self.validate_resources:
                        warnings.extend("%s: %s" % (obj.name, item)
                                        for item in validate_dependencies(scene_directory, mesh_path))
                entity.set(ns("detailA"), str(int(obj.get("ymgre_detail_a", 20))))
                entity.set(ns("detailB"), str(int(obj.get("ymgre_detail_b", 20))))
                color = obj.color[:3]
                entity.set(ns("color"), "%02X%02X%02X" % tuple(max(0, min(255, round(c * 255))) for c in color))
            elif obj.type == "LIGHT":
                if obj.data.type not in {"POINT", "SPOT"}:
                    warnings.append("%s: only point and spot lights are exported" % obj.name)
                light_type = "spot" if obj.data.type == "SPOT" else "point"
                light = ET.SubElement(node, "light", name=obj.name, type=light_type,
                                      castShadows="true" if obj.data.use_shadow else "false",
                                      powerScale="%.6g" % max(0.0, obj.data.energy / 1000.0))
                ET.SubElement(light, "colourDiffuse", **color_attributes(obj.data.color))
                tx, ty, tz = object_target(obj)
                ET.SubElement(light, ns("target"), x="%.9g" % tx, y="%.9g" % ty, z="%.9g" % tz)
            else:
                camera = ET.SubElement(node, "camera", name=obj.name, projectionType="perspective",
                                       fov="%.9g" % math.degrees(obj.data.angle))
                ET.SubElement(camera, "clipping", near="%.9g" % obj.data.clip_start,
                              far="%.9g" % obj.data.clip_end)
                tx, ty, tz = object_target(obj)
                ET.SubElement(camera, ns("target"), x="%.9g" % tx, y="%.9g" % ty, z="%.9g" % tz)
            exported.append((obj, node_id))

        if virtual_camera:
            node_id = "ymgre-node-%d" % next_index
            node = ET.SubElement(nodes, "node", name="Main Camera", id=node_id)
            ET.SubElement(node, "position", x="0", y="149", z="-213")
            ET.SubElement(node, "rotation", qx="0", qy="0", qz="0", qw="1")
            ET.SubElement(node, "scale", x="1", y="1", z="1")
            data = ET.SubElement(node, "userData")
            for name, value in (("fixed", "false"), ("visible", "true"), ("wireframe", "false")):
                ET.SubElement(data, "property", name=name, type="bool", data=value)
            camera = ET.SubElement(node, "camera", name="Main Camera", projectionType="perspective", fov="45")
            ET.SubElement(camera, "clipping", near="0.01", far="1000")
            ET.SubElement(camera, ns("target"), x="0", y="0", z="0")
            main_id = node_id
        else:
            active = scene.camera if scene.camera in cameras else cameras[0]
            main_id = next(node_id for obj, node_id in exported if obj == active)
        root.set(ns("mainCamera"), main_id)
        root.set(ns("activeCamera"), main_id)

        ET.indent(root, space="\t")
        ET.ElementTree(root).write(self.filepath, encoding="utf-8", xml_declaration=True)
        if warnings:
            self.report({"WARNING"}, "Exported with %d resource warning(s); see console" % len(warnings))
            for warning in warnings:
                print("YMGRE export:", warning)
        else:
            self.report({"INFO"}, "YMGRE scene exported")
        return {"FINISHED"}


def export_menu(self, context):
    self.layout.operator(ExportYMGREScene.bl_idname, text="YMGRE Scene (.scene)")


def register():
    bpy.utils.register_class(ExportYMGREScene)
    bpy.types.TOPBAR_MT_file_export.append(export_menu)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(export_menu)
    bpy.utils.unregister_class(ExportYMGREScene)


if __name__ == "__main__":
    register()
