use anyhow::{Context, Result};
use pyo3::prelude::*;
use pyo3::types::{PyBytes, PyModule};
use std::ffi::CString;
use std::path::Path;

pub fn custom_script(buffer: &[u8]) -> Result<u32> {
    if !Path::new("script.py").exists() {
        anyhow::bail!("Script file 'script.py' not found in the current directory!");
    }

    Python::with_gil(|py| {
        // Read Python script file
        let script_content =
            std::fs::read_to_string("script.py").context("Failed to read script.py")?;

        let filename = CString::new("script.py")
            .map_err(|e| anyhow::anyhow!("Failed to create CString for filename: {}", e))?;
        let module_name = CString::new("script")
            .map_err(|e| anyhow::anyhow!("Failed to create CString for module name: {}", e))?;
        let script = CString::new(script_content)
            .map_err(|e| anyhow::anyhow!("Failed to create CString for script content: {}", e))?;

        // Compile the python script
        let module = PyModule::from_code(py, &script, &filename, &module_name)
            .context("Failed to compile Python script")?;

        // Get the function
        let analyse_func = module
            .getattr("analyse")
            .context("Function 'analyse' not found in script")?;

        // Convert the u8 buffer to PyBytes
        let py_buffer = PyBytes::new(py, buffer);

        // Call analyse(buffer)
        let result = analyse_func
            .call1((py_buffer,))
            .context("Error calling 'analyse' function")?;

        // Fetch the port number from the result and return it.
        let port: u32 = result
            .extract()
            .context("Failed to extract port number from Python result")?;

        Ok(port)
    })
}
