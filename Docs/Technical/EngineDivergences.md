# Engine Divergences

All engine divergences are wrapped in code tags to facilitate tracking. Code tags look like:

```
// Unmodified engine code...

// TEXTURESET_START - {date} - {user} - [DIVERGENCE] {description}
// Modified engine code
// TEXTURESET_END

// ... Unmodified engine code
```

Searching the codebase for `TEXTURESET_START` will allow you to find all instances of engine divergences.