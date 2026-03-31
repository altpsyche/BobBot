---
paths:
  - "**/*PCG*"
  - "**/PCG/**"
---

# PCG (Procedural Content Generation)

## Creating a PCG graph

Use `create_pcg_graph(name, path)` to create a new graph asset.
Use `get_pcg_graph_info(graph_path)` to inspect settings and nodes.

## Executing PCG

Use `execute_pcg_graph(actor_label)` on a PCGVolume actor in the level to regenerate.
Always check `get_pcg_volumes()` first to find existing PCG actors.

## Tips

- PCG graphs execute on the actor they're attached to — always target the volume/actor
- Regeneration can be expensive; use `benchmark_scene()` to measure impact
- PCG output goes to the transient level by default unless configured otherwise
