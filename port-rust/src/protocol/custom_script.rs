use pyo3::prelude::*;
use pyo3::types::{PyModule, PyBytes};
use std::path::Path;
use std::ffi::CString;

pub fn custom_script(buffer: &[u8]) -> Result<u32, ()> {
    if !Path::new("script.py").exists() { //path is hardcoded
        eprintln!("Script file not found in the current directory!");
        return Err(());
    }

    // GIL = Global Interpreter Lock
    Python::with_gil(|py| {
        // Read Python script file
        let script_content = match std::fs::read_to_string("script.py") { //path is hardcoded
            Ok(content) => content,
            Err(e) => {
                eprintln!("Failed to read script: {}", e);
                return Err(());
            }
        };

        let filename = CString::new("script.py").unwrap();  //hardcoded
        let module_name = CString::new("script").unwrap();  //hardcoded
        let code = CString::new(script_content).unwrap();

        // Compile the python script
        let module = match PyModule::from_code(py, &code, &filename, &module_name) {
            Ok(m) => m,
            Err(e) => {
                eprintln!("Failed to compile Python script: {}", e);
                return Err(());
            }
        };

        // Get the function
        let analyse_func = match module.getattr("analyse") {
            Ok(f) => f,
            Err(e) => {
                eprintln!("Function 'analyse' not found: {}", e);
                return Err(());
            }
        };

        // Convert the u8 buffer to PyBytes
        let py_buffer = PyBytes::new(py, buffer);

        // Call analyse(buffer)
        let result = match analyse_func.call1((py_buffer,)) {
            Ok(r) => r,
            Err(e) => {
                eprintln!("Error calling analyse: {}", e);
                return Err(());
            }
        };

        // Fetch the port number from the result and return it.
        match result.extract::<u32>() {
            Ok(port) => Ok(port),
            Err(e) => {
                eprintln!("Failed to parse port number{}", e);
                Err(())
            }
        }
    })
}