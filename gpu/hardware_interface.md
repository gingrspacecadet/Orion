# Lyra Hardware Interface

## Command Processor

Lyra contains a command processor responsible for executing command buffers.

The CPU communicates with the GPU through MMIO registers.

---

## Registers

| Offset | Name | Description |
|----------|------|-------------|
| 0x00 | COMMAND_BUFFER_BASE | Physical address of command buffer |
| 0x04 | COMMAND_BUFFER_SIZE | Size in bytes |
| 0x08 | CONTROL | Starts execution |
| 0x0C | STATUS | GPU status bits |
| 0x10 | COMPLETED_FENCE | Last completed fence value |

---

## Submission

The CPU constructs a command buffer in memory.

The CPU writes:

- COMMAND_BUFFER_BASE
- COMMAND_BUFFER_SIZE

and then sets the START bit in CONTROL.

The GPU fetches and executes commands asynchronously.

---

## Fence Completion

When a fence packet completes, the GPU updates:

    COMPLETED_FENCE

with the fence identifier.

Software may:

- poll COMPLETED_FENCE
- wait for an interrupt

---

## Interrupts

The GPU may generate hardware interrupts.

Supported interrupt sources include:

- VBLANK
- Fence completion
- Fault conditions

Interrupts are delivered through the ICU.