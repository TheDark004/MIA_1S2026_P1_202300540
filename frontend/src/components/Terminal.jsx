// Props:
//   inputText     -> texto del textarea superior (comandos)
//   outputText    -> texto del textarea inferior (resultados)
//   isLoading     -> booleano, true mientras el backend procesa
//   onInputChange -> callback cuando el usuario escribe
//   onExecute     -> callback del botón "Ejecutar"
//   onLoadScript  -> callback del botón "Cargar Script"
//   onClearOutput -> callback del botón "Limpiar"

import { useEffect, useRef } from "react"

export default function Terminal({
  inputText, outputText, isLoading,
  onInputChange, onExecute, onLoadScript, onClearOutput
}) {

  // Auto-scroll al final del output cuando llega nuevo contenido
  const outputRef = useRef(null)
  useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight
    }
  }, [outputText])

  // Ejecutar con Ctrl+Enter (además del botón)
  const handleKeyDown = (e) => {
    if (e.ctrlKey && e.key === "Enter") {
      onExecute()
    }
  }

  return (
    <div style={styles.container}>

      {/* ── Panel izquierdo: ENTRADA ──────────────────────── */}
      <div style={styles.panel}>
        <div style={styles.panelHeader}>
          <span style={styles.panelLabel}>Entrada de Comandos</span>
          <span style={styles.hint}>Ctrl+Enter para ejecutar</span>
        </div>

        <textarea
          style       = {styles.textarea}
          value       = {inputText}
          onChange    = {(e) => onInputChange(e.target.value)}
          onKeyDown   = {handleKeyDown}
          placeholder = {"# Escribe tus comandos aquí\nmkdisk -size=10 -unit=m -fit=ff -path=/home/user/disco.mia\nfdisk -size=5 -unit=m -name=part1 -path=/home/user/disco.mia\nmount -path=/home/user/disco.mia -name=part1\nmkfs -id=401A"}
          spellCheck  = {false}
        />

        {/* Botones de acción */}
        <div style={styles.buttonRow}>
          <button
            style     = {{
              ...styles.button,
              ...styles.btnExecute,
              opacity: isLoading ? 0.6 : 1,
            }}
            onClick   = {onExecute}
            disabled  = {isLoading}
          >
            {isLoading ? "⏳ Ejecutando..." : "▶ Ejecutar"}
          </button>

          <button
            style   = {{...styles.button, ...styles.btnLoad}}
            onClick = {onLoadScript}
          >
          Cargar Script (.smia) 📂
          </button>

          <button
            style   = {{...styles.button, ...styles.btnClear}}
            onClick = {() => onInputChange("")}
          >
             Limpiar Entrada
          </button>
        </div>
      </div>

      {/* ── Panel derecho: SALIDA ─────────────────────────── */}
      <div style={styles.panel}>
        <div style={styles.panelHeader}>
          <span style={styles.panelLabel}>📤 Salida</span>
          <button
            style   = {{...styles.button, ...styles.btnSmall}}
            onClick = {onClearOutput}
          >
            Limpiar
          </button>
        </div>

        <textarea
          ref       = {outputRef}
          style     = {{...styles.textarea, ...styles.outputArea}}
          value     = {outputText}
          readOnly  = {true}
          spellCheck= {false}
        />
      </div>

    </div>
  )
}

// ── Estilos 
const styles = {
  container: {
    display:   "grid",
    gridTemplateColumns: "1fr 1fr",  // dos columnas iguales
    gap:       "20px",
    height:    "calc(100vh - 120px)",
  },
  panel: {
    display:       "flex",
    flexDirection: "column",
    gap:           "10px",
  },
  panelHeader: {
    display:        "flex",
    justifyContent: "space-between",
    alignItems:     "center",
    padding:        "8px 12px",
    backgroundColor:"#161b22",
    borderRadius:   "6px",
    border:         "1px solid #30363d",
  },
  panelLabel: {
    color:      "#58a6ff",
    fontWeight: "bold",
    fontSize:   "0.9rem",
  },
  hint: {
    color:    "#8b949e",
    fontSize: "0.75rem",
  },
  textarea: {
    flex:            1,
    backgroundColor: "#0d1117",
    color:           "#c9d1d9",
    border:          "1px solid #30363d",
    borderRadius:    "6px",
    padding:         "14px",
    fontFamily:      "'Fira Code', 'Courier New', monospace",
    fontSize:        "0.85rem",
    resize:          "none",
    outline:         "none",
    lineHeight:      "1.6",
  },
  outputArea: {
    color:           "#7ee787",  // verde para el output (estilo terminal)
    backgroundColor: "#0a0f14",
  },
  buttonRow: {
    display: "flex",
    gap:     "10px",
    flexWrap:"wrap",
  },
  button: {
    cursor:          "pointer",
    border:          "none",
    borderRadius:    "6px",
    padding:         "8px 16px",
    fontFamily:      "'Fira Code', monospace",
    fontSize:        "0.85rem",
    fontWeight:      "bold",
    transition:      "opacity 0.2s",
  },
  btnExecute: {
    backgroundColor: "#238636",
    color:           "#ffffff",
    flex:            1,
  },
  btnLoad: {
    backgroundColor: "#1f6feb",
    color:           "#ffffff",
  },
  btnClear: {
    backgroundColor: "#30363d",
    color:           "#c9d1d9",
  },
  btnSmall: {
    backgroundColor: "#30363d",
    color:           "#8b949e",
    padding:         "4px 10px",
    fontSize:        "0.75rem",
  },
}