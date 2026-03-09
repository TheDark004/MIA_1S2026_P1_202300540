import { useState, useRef } from "react"
import Terminal from "./components/Terminal"

// URL del backend C++ 
const BACKEND_URL = ""

export default function App() {

  // Estado principal
  // inputText  -> lo que está escrito en el textarea de entrada
  // outputText -> lo que muestra el textarea de salida
  // isLoading  -> true mientras espera respuesta del backend
  const [inputText,  setInputText]  = useState("")
  const [outputText, setOutputText] = useState("// Bienvenido a ExtreamFS\n// Escribe comandos o carga un script .smia\n")
  const [isLoading,  setIsLoading]  = useState(false)

  // Referencia al input de archivo (para el botón "Cargar Script")
  const fileInputRef = useRef(null)

  // Ejecutar comandos
  // Envía el contenido del textarea al backend via POST /execute
  const handleExecute = async () => {
    if (!inputText.trim()) return
    setIsLoading(true)

    try {
      const response = await fetch(`${BACKEND_URL}/execute`, {
        method:  "POST",
        headers: { "Content-Type": "application/json" },
        // Enviamos el texto completo — puede ser 1 línea o todo un script
        body: JSON.stringify({ commands: inputText }),
      })

      const data = await response.json()

      // Acumular el output (no reemplazar, agregar al final)
      setOutputText(prev => prev + "\n" + data.output)

    } catch (err) {
      // Si el backend no responde (no está corriendo)
      setOutputText(prev =>
        prev + "\nError: No se pudo conectar al backend.\n" +
        "Asegúrate de que ExtreamFS esté corriendo en puerto 18080.\n"
      )
    } finally {
      setIsLoading(false)
    }
  }

  //  Cargar script .smia 
  // Lee el archivo seleccionado y lo vuelca en el textarea de entrada
  const handleLoadScript = (event) => {
    const file = event.target.files[0]
    if (!file) return

    const reader = new FileReader()
    reader.onload = (e) => {
      // El contenido del archivo va al textarea de entrada
      setInputText(e.target.result)
      setOutputText(prev => prev + `\n// Script cargado: ${file.name}\n`)
    }
    reader.readAsText(file)

    // Limpiar el input para poder cargar el mismo archivo de nuevo
    event.target.value = ""
  }

  // Limpiar salida 
  const handleClearOutput = () => {
    setOutputText("")
  }

  return (
    <div style={styles.app}>
      {/* Header */}
      <header style={styles.header}>
        <h1 style={styles.title}>⚡ ExtreamFS</h1>
        <span style={styles.subtitle}>Simulador de Sistema de Archivos EXT2</span>
      </header>

      {/* Área principal */}
      <main style={styles.main}>
        <Terminal
          inputText      = {inputText}
          outputText     = {outputText}
          isLoading      = {isLoading}
          onInputChange  = {(val) => setInputText(val)}
          onExecute      = {handleExecute}
          onLoadScript   = {() => fileInputRef.current.click()}
          onClearOutput  = {handleClearOutput}
        />
      </main>

      {/* Input de archivo oculto */}
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
    backgroundColor: "#0d1117",
    color:           "#c9d1d9",
    fontFamily:      "'Fira Code', 'Courier New', monospace",
    display:         "flex",
    flexDirection:   "column",
  },
  header: {
    backgroundColor: "#161b22",
    borderBottom:    "1px solid #30363d",
    padding:         "16px 24px",
    display:         "flex",
    alignItems:      "center",
    gap:             "16px",
  },
  title: {
    margin: 0,
    fontSize: "1.4rem",
    color: "#58a6ff",
  },
  subtitle: {
    color:    "#8b949e",
    fontSize: "0.9rem",
  },
  main: {
    flex:    1,
    padding: "20px",
  },
}