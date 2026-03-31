---
paths:
  - "**/*Skeleton*"
  - "**/*Skeletal*"
  - "**/*Socket*"
---

# Skeletal Mesh & Sockets

## Inspecting skeletons

- `get_skeleton_info(skeleton_path)` — bone hierarchy, socket list
- `get_skeletal_mesh_info(mesh_path)` — LODs, materials, morph targets

## Creating sockets

Use `create_socket(skeleton_path, bone_name, socket_name)` to add an attachment point.
Sockets are attachment points on bones for weapons, VFX, etc.

## Attaching actors to sockets

Use `attach_actor_to_socket(actor_label, parent_label, socket_name)` to snap an actor to a socket.

## Common bone names

- `root`, `pelvis`, `spine_01/02/03`
- `neck_01`, `head`
- `clavicle_l/r`, `upperarm_l/r`, `lowerarm_l/r`, `hand_l/r`
- `thigh_l/r`, `calf_l/r`, `foot_l/r`

## Tips

- Socket names must be unique per skeleton
- Sockets inherit the bone's transform — use relative offsets for positioning
- `attach_actor_to_socket` is commonly used for weapon attachment
- Use `get_skeleton_animations(skeleton_path)` to find compatible animations
