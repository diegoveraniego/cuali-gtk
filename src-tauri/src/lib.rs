// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/
use std::process::{Command, Child, Stdio};
use std::sync::{Arc, Mutex};
use std::net::TcpStream;
use std::time::Duration;
use std::io::{BufRead, BufReader};
use tauri::Manager;

struct AppState {
    child: Arc<Mutex<Option<Child>>>,
    token: Arc<Mutex<Option<String>>>,
}

#[tauri::command]
fn get_server_url(state: tauri::State<AppState>) -> String {
    let token = state.token.lock().unwrap();
    if let Some(t) = &*token {
        format!("http://localhost:7465/?token={}", t)
    } else {
        "http://localhost:7465".to_string()
    }
}

#[tauri::command]
fn check_server() -> bool {
    TcpStream::connect_timeout(&"127.0.0.1:7465".parse().unwrap(), Duration::from_millis(100)).is_ok()
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let child_state = Arc::new(Mutex::new(None));
    let token_state = Arc::new(Mutex::new(None));
    
    let child_state_clone = child_state.clone();
    let token_state_clone = token_state.clone();

    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .manage(AppState {
            child: child_state,
            token: token_state,
        })
        .setup(move |app| {
            let home = std::env::var("HOME").unwrap();
            let taguette_path = format!("{}/.local/bin/taguette", home);
            
            let mut child = Command::new(taguette_path)
                .arg("--no-browser")
                .stdout(Stdio::piped())
                .stderr(Stdio::piped())
                .env("PYTHONUNBUFFERED", "1")
                .spawn()
                .expect("Failed to start taguette");

            let stdout = child.stdout.take().unwrap();
            let stderr = child.stderr.take().unwrap();
            
            {
                let mut state = child_state_clone.lock().unwrap();
                *state = Some(child);
            }

            // Thread to parse token from stdout/stderr
            let token_clone = token_state_clone.clone();
            std::thread::spawn(move || {
                let reader = BufReader::new(stdout);
                for line in reader.lines() {
                    if let Ok(l) = line {
                        println!("TAGUETTE STDOUT: {}", l);
                        if l.contains("token=") {
                            if let Some(pos) = l.find("token=") {
                                let t = &l[pos + 6..];
                                // Extract until whitespace or end
                                let t = t.split_whitespace().next().unwrap_or(t);
                                let mut token_lock = token_clone.lock().unwrap();
                                *token_lock = Some(t.to_string());
                                println!("Found token: {}", t);
                            }
                        }
                    }
                }
            });

            let token_clone_err = token_state_clone.clone();
            std::thread::spawn(move || {
                let reader = BufReader::new(stderr);
                for line in reader.lines() {
                    if let Ok(l) = line {
                        println!("TAGUETTE STDERR: {}", l);
                        if l.contains("token=") {
                            if let Some(pos) = l.find("token=") {
                                let t = &l[pos + 6..];
                                let t = t.split_whitespace().next().unwrap_or(t);
                                let mut token_lock = token_clone_err.lock().unwrap();
                                *token_lock = Some(t.to_string());
                                println!("Found token (stderr): {}", t);
                            }
                        }
                    }
                }
            });
// Inject Adwaita CSS on every page load
let adwaita_css = include_str!("../../src/adwaita.css");
let script = format!(
    r#"
    (function() {{
        const injectStyle = () => {{
            if (document.getElementById('adwaita-styling')) return;
            const style = document.createElement('style');
            style.id = 'adwaita-styling';
            style.textContent = `{}`;
            document.head ? document.head.append(style) : document.documentElement.append(style);
            console.log('Adwaita styles injected');
        }};

        // Inject immediately
        if (document.readyState === 'loading') {{
            document.addEventListener('DOMContentLoaded', injectStyle);
        }} else {{
            injectStyle();
        }}

        // Re-inject if head is replaced or on some transitions
        const observer = new MutationObserver(() => {{
            if (!document.getElementById('adwaita-styling')) {{
                injectStyle();
            }}
        }});
        observer.observe(document.documentElement, {{ childList: true, subtree: true }});
    }})();
    "#,
    adwaita_css.replace('`', "\\`").replace('$', "\\$")
);

            
            let _ = tauri::WebviewWindowBuilder::new(app, "main", tauri::WebviewUrl::App("index.html".into()))
                .title("Taguette")
                .initialization_script(&script)
                .inner_size(1024.0, 768.0)
                .build();

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![check_server, get_server_url])
        .on_window_event(|window, event| {
            if let tauri::WindowEvent::CloseRequested { .. } = event {
                let state = window.state::<AppState>();
                let mut lock = state.child.lock().unwrap();
                if let Some(mut child) = lock.take() {
                    let _ = child.kill();
                }
            }
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
