# Frion and Fridge Assembly

This VS Code extension highlights Frion (CPM) files (`.cpm`, `.cinc`, `.fri`) and Fridge Assembly files (`.falc`, `.x2al`).

Open this directory in VS Code and press **F5** to launch an Extension Development Host. In that window, open a CPM or FALC source file.

For assembly includes, select **Fridge Assembly (FALC)** from **Change Language Mode**, or add an association to the workspace settings.

```json
{
    "files.associations": {
        "*.inc": "falc"
    }
}
```
