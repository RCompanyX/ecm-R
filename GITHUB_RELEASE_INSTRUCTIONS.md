# Instrucciones para Crear Pre-Release en GitHub

## Método 1: Desde la Interfaz Web de GitHub (Recomendado)

1. **Ve a Releases:**
   - Accede a: `https://github.com/RCompanyX/ecm-r/releases`

2. **Click en "Draft a new release"**

3. **Rellena los campos:**

   - **Tag version:** `v0.5.4-alpha` (ya existe en el repo)
   - **Target:** `main` (o `dev_previous-track-control` si prefieres)
   - **Release title:** `v0.5.4-alpha - Previous Track Navigation Control`

   - **Description:** Copia el contenido completo de `RELEASE_v0.5.4-alpha.md`

4. **Marca como Pre-release:**
   - ☑️ "Set as a pre-release"

5. **Publish Release**
   - Click en "Publish release"

---

## Método 2: Usando GitHub CLI (si está disponible)

```bash
gh release create v0.5.4-alpha \
  --title "v0.5.4-alpha - Previous Track Navigation Control" \
  --notes-file RELEASE_v0.5.4-alpha.md \
  --prerelease
```

---

## Método 3: Via REST API (cURL)

```bash
curl -X POST \
  -H "Authorization: token YOUR_GITHUB_TOKEN" \
  -H "Accept: application/vnd.github.v3+json" \
  https://api.github.com/repos/RCompanyX/ecm-r/releases \
  -d '{
    "tag_name": "v0.5.4-alpha",
    "target_commitish": "main",
    "name": "v0.5.4-alpha - Previous Track Navigation Control",
    "body": "PASTE_CONTENT_FROM_RELEASE_v0.5.4-alpha.md_HERE",
    "draft": false,
    "prerelease": true
  }'
```

---

## Contenido Preparado

El contenido de la pre-release está en: **`RELEASE_v0.5.4-alpha.md`**

Incluye:
- ✅ Resumen ejecutivo
- ✅ Características nuevas
- ✅ Cambios técnicos
- ✅ Instrucciones de uso
- ✅ Ejemplos de comportamiento
- ✅ Notas importantes
- ✅ Información de testing
- ✅ Enlaces a commits y comparativos

---

## Información del Release

| Campo | Valor |
|-------|-------|
| **Tag** | `v0.5.4-alpha` |
| **Tipo** | Pre-release (Alpha) |
| **Rama** | `main` |
| **Commits** | 2 (ac789f5, b65e36f) |
| **Fecha** | 2026-04-27 |

---

## Links Útiles

- **Release Page:** https://github.com/RCompanyX/ecm-r/releases/tag/v0.5.4-alpha
- **Compare:** https://github.com/RCompanyX/ecm-r/compare/v0.5.3-alpha...v0.5.4-alpha
- **Tag:** https://github.com/RCompanyX/ecm-r/releases/new

