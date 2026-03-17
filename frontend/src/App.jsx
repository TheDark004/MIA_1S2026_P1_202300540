import { useState, useRef } from "react"
import Terminal from "./components/Terminal"
import ReportGallery from "./components/ReportGallery"

const BACKEND_URL = ""

export default function App() {
  const [inputText,  setInputText]  = useState("")
  const [outputText, setOutputText] = useState("")
  const [isLoading,  setIsLoading]  = useState(false)
  const [reports,    setReports]    = useState([]) // lista de rutas de reportes
  const [showGallery, setShowGallery] = useState(false)
  const fileInputRef = useRef(null)

  const handleExecute = async () => {
    if (!inputText.trim()) return
    setIsLoading(true)
    try {
      const response = await fetch(`${BACKEND_URL}/execute`, {
        method:  "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ commands: inputText }),
      })
      const data = await response.json()
      const output = data.output || ""

      // Extraer rutas de reportes del output
      const newReports = []
      const lines = output.split("\n")
      lines.forEach(line => {
        if (line.startsWith("REPORTE:")) {
          const ruta = line.replace("REPORTE:", "").trim()
          // Convertir ruta absoluta a URL del backend
          const filename = ruta.split("/").pop()
          newReports.push({ filename, ruta, url: `/reports/${encodeURIComponent(ruta)}` })
        }
      })

      if (newReports.length > 0) {
        setReports(prev => [...prev, ...newReports])
      }

      // Filtrar líneas REPORTE: del output visible
      const cleanOutput = lines
        .filter(l => !l.startsWith("REPORTE:"))
        .join("\n")

      setOutputText(prev => prev + "\n" + cleanOutput)

    } catch (err) {
      setOutputText(prev =>
        prev + "\nError: No se pudo conectar al backend.\n"
      )
    } finally {
      setIsLoading(false)
    }
  }

  const handleLoadScript = (event) => {
    const file = event.target.files[0]
    if (!file) return
    const reader = new FileReader()
    reader.onload = (e) => {
      setInputText(e.target.result)
      setOutputText(prev => prev + `\n// Script cargado: ${file.name}\n`)
    }
    reader.readAsText(file)
    event.target.value = ""
  }

  return (
    <div style={styles.app}>
      {/* Header */}
      <header style={styles.header}>
        <div style={styles.headerLeft}>
          <div style={styles.logo}>
            <span style={styles.logoIcon}>⬡</span>
            <span style={styles.logoText}>ExtreamFS</span>
          </div>
          <span style={styles.subtitle}>EXT2 Filesystem Simulator</span>
        </div>

        <div style={styles.headerRight}>
          {reports.length > 0 && (
            <button
              style={styles.galleryBtn}
              onClick={() => setShowGallery(true)}
            >
              <span style={styles.galleryBtnDot}>{reports.length}</span>
              Ver Reportes
            </button>
          )}
        </div>
      </header>

      {/* Terminal */}
      <main style={styles.main}>
        <Terminal
          inputText     = {inputText}
          outputText    = {outputText}
          isLoading     = {isLoading}
          onInputChange = {(val) => setInputText(val)}
          onExecute     = {handleExecute}
          onLoadScript  = {() => fileInputRef.current.click()}
          onClearOutput = {() => setOutputText("")}
          onClearInput  = {() => setInputText("")}
        />
      </main>

      {/* Barra inferior de reportes */}
      {reports.length > 0 && !showGallery && (
        <div style={styles.reportBar}>
          <span style={styles.reportBarLabel}>
            📊 {reports.length} reporte{reports.length > 1 ? "s" : ""} generado{reports.length > 1 ? "s" : ""}
          </span>
          <div style={styles.reportChips}>
            {reports.slice(-4).map((r, i) => (
              <span
                key={i}
                style={styles.chip}
                onClick={() => setShowGallery(true)}
              >
                {r.filename}
              </span>
            ))}
            {reports.length > 4 && (
              <span style={styles.chipMore}>+{reports.length - 4} más</span>
            )}
          </div>
          <button style={styles.viewAllBtn} onClick={() => setShowGallery(true)}>
            Ver todos →
          </button>
        </div>
      )}

      {/* Galería modal */}
      {showGallery && (
        <ReportGallery
          reports={reports}
          onClose={() => setShowGallery(false)}
          onClear={() => { setReports([]); setShowGallery(false) }}
        />
      )}

      {/* Input oculto para cargar scripts */}
      <input
        ref      = {fileInputRef}
        type     = "file"
        accept   = ".smia,.txt"
        onChange = {handleLoadScript}
        style    = {{ display: "none" }}
      />
    </div>
  )
}

const styles = {
  app: {
    minHeight:       "100vh",
    backgroundColor: "#2d3748",
    background:      "linear-gradient(135deg, #2a3548 0%, #1e2a3a 100%)",
    color:           "#E9E9EB",
    fontFamily:      "'IBM Plex Mono', 'Fira Code', monospace",
    display:         "flex",
    flexDirection:   "column",
  },
  header: {
    backgroundColor: "rgba(67, 80, 108, 0.8)",
    backdropFilter:  "blur(12px)",
    borderBottom:    "1px solid rgba(61, 97, 155, 0.4)",
    padding:         "0 28px",
    height:          "60px",
    display:         "flex",
    alignItems:      "center",
    justifyContent:  "space-between",
  },
  headerLeft: {
    display:    "flex",
    alignItems: "center",
    gap:        "20px",
  },
  logo: {
    display:    "flex",
    alignItems: "center",
    gap:        "8px",
  },
  logoIcon: {
    fontSize:  "1.4rem",
    color:     "#EF4B4C",
    lineHeight: 1,
  },
  logoText: {
    fontSize:    "1.1rem",
    fontWeight:  "700",
    color:       "#E9E9EB",
    letterSpacing: "0.05em",
  },
  subtitle: {
    fontSize:   "0.75rem",
    color:      "rgba(233, 233, 235, 0.4)",
    borderLeft: "1px solid rgba(61, 97, 155, 0.5)",
    paddingLeft: "20px",
    letterSpacing: "0.08em",
    textTransform: "uppercase",
  },
  headerRight: {
    display:    "flex",
    alignItems: "center",
    gap:        "12px",
  },
  galleryBtn: {
    display:         "flex",
    alignItems:      "center",
    gap:             "8px",
    backgroundColor: "rgba(61, 97, 155, 0.3)",
    border:          "1px solid rgba(61, 97, 155, 0.6)",
    borderRadius:    "8px",
    color:           "#E9E9EB",
    padding:         "6px 14px",
    fontSize:        "0.8rem",
    cursor:          "pointer",
    fontFamily:      "inherit",
    transition:      "all 0.2s",
  },
  galleryBtnDot: {
    backgroundColor: "#EF4B4C",
    color:           "white",
    borderRadius:    "50%",
    width:           "18px",
    height:          "18px",
    display:         "flex",
    alignItems:      "center",
    justifyContent:  "center",
    fontSize:        "0.7rem",
    fontWeight:      "700",
  },
  main: {
    flex:    1,
    padding: "20px 28px",
    minHeight: 0,
  },
  reportBar: {
    backgroundColor: "rgba(67, 80, 108, 0.9)",
    backdropFilter:  "blur(8px)",
    borderTop:       "1px solid rgba(61, 97, 155, 0.4)",
    padding:         "10px 28px",
    display:         "flex",
    alignItems:      "center",
    gap:             "16px",
  },
  reportBarLabel: {
    fontSize:    "0.8rem",
    color:       "rgba(233, 233, 235, 0.6)",
    whiteSpace:  "nowrap",
  },
  reportChips: {
    display: "flex",
    gap:     "8px",
    flex:    1,
    overflow: "hidden",
  },
  chip: {
    backgroundColor: "rgba(61, 97, 155, 0.3)",
    border:          "1px solid rgba(61, 97, 155, 0.5)",
    borderRadius:    "4px",
    padding:         "2px 10px",
    fontSize:        "0.72rem",
    color:           "#E9E9EB",
    cursor:          "pointer",
    whiteSpace:      "nowrap",
  },
  chipMore: {
    fontSize: "0.72rem",
    color:    "rgba(233, 233, 235, 0.4)",
    padding:  "2px 6px",
  },
  viewAllBtn: {
    backgroundColor: "transparent",
    border:          "none",
    color:           "#3D619B",
    fontSize:        "0.8rem",
    cursor:          "pointer",
    fontFamily:      "inherit",
    whiteSpace:      "nowrap",
    padding:         "0",
  },
}