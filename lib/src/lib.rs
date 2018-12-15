#[macro_use]
extern crate lazy_static;
extern crate dmd_core;
extern crate libc;

use libc::*;
use std::sync::Mutex;
use std::ptr;
use dmd_core::dmd::Dmd;
use dmd_core::err::DuartError;

struct DmdState {
    pub dmd: Option<Dmd>,
}

impl DmdState {
    pub fn new() -> DmdState {
        DmdState {
            dmd: None,
        }
    }
}

lazy_static! {
    static ref DMD_STATE: Mutex<DmdState> = Mutex::new(DmdState::new());
}

#[no_mangle]
fn dmd_init() -> bool {
    match DMD_STATE.lock() {
        Ok(mut dmd_guard) => {
            match dmd_guard.dmd {
                Some(_) => {
                    println!("[DMD ERROR] DMD already initialized");
                    false
                }
                None => {
                    dmd_guard.dmd = Some(Dmd::new());
                    println!("[DMD] Created new DMD");
                    true
                }
            }
        }
        Err(e) => {
            println!("[DMD ERROR] Could not lock DMD. Error={:?}", e);
            false
        }
    }
}

#[no_mangle]
fn dmd_reset() -> bool {
    match DMD_STATE.lock() {
        Ok(mut dmd_guard) => {
            match &mut dmd_guard.dmd {
                Some(dmd) => {
                    match dmd.reset() {
                        Ok(()) => {
                            println!("[DMD] Reset Success.");
                            true
                        },
                        Err(e) => {
                            println!("[DMD ERROR] DMD reset error: {:?}", e);
                            false
                        }
                    }
                },
                None => {
                    println!("[DMD ERROR] DMD not initialized.");
                    false
                }
            }
        },
        Err(e) => {
            println!("[DMD ERROR] Could not lock DMD. Error={:?}", e);
            false
        }
    }
}

#[no_mangle]
fn dmd_video_ram() -> *const u8 {
    match DMD_STATE.lock() {
        Ok(mut dmd_guard) => {
            match &mut dmd_guard.dmd {
                Some(dmd) => {
                    match dmd.video_ram() {
                        Ok(video_ram) => {
                            let ptr: *const u8 = video_ram.as_ptr();
                            return ptr;
                        },
                        Err(e) => {
                            println!("DMD video ram error: {:?}", e);
                            return ptr::null();
                        }
                    }
                },
                None => {
                    println!("DMD not initialized.");
                    return ptr::null();
                }
            }
        },
        Err(e) => {
            println!("Could not lock DMD. Error={:?}", e);
            return ptr::null();
        }
    }
}

#[no_mangle]
fn dmd_step() -> bool {
    match DMD_STATE.lock() {
        Ok(mut dmd_guard) => {
            match &mut dmd_guard.dmd {
                Some(dmd) => {
                    match dmd.step_with_error() {
                        Ok(()) => {
                            true
                        },
                        Err(e) => {
                            println!("Could not step DMD. PC=0x{:08x}, Error={:?}", dmd.get_pc(), e);
                            false
                        }
                    }
                }
                None => {
                    println!("DMD not initialized.");
                    false
                }
            }
        },
        Err(e) => {
            println!("Could not lock DMD. Error={:?}", e);
            false
        }
    }
}

#[no_mangle]
fn dmd_get_pc() -> uint32_t {
    match DMD_STATE.lock() {
        Ok(mut dmd_guard) => {
            match &mut dmd_guard.dmd {
                Some(dmd) => {
                    dmd.get_pc()
                }
                None => {
                    println!("DMD not initialized.");
                    0
                }
            }
        },
        Err(e) => {
            println!("Could not lock DMD. Error={:?}", e);
            0
        }
    }
}

#[no_mangle]
fn dmd_get_duart_output_port() -> uint8_t {
    match DMD_STATE.lock() {
        Ok(mut dmd_guard) => {
            match &mut dmd_guard.dmd {
                Some(dmd) => {
                    dmd.duart_output()
                }
                None => {
                    println!("DMD not initialized.");
                    0
                }
            }
        },
        Err(e) => {
            println!("Could not lock DMD. Error={:?}", e);
            0
        }
    }
}

#[no_mangle]
fn dmd_rx_char(c: uint8_t) -> bool {
    match DMD_STATE.lock() {
        Ok(mut dmd_guard) => {
            match &mut dmd_guard.dmd {
                Some(dmd) => {
                    match dmd.rx_char(c as u8) {
                        Ok(()) => {
                            true
                        },
                        Err(DuartError::ReceiverNotReady) => {
                            false
                        }
                    }
                }
                None => {
                    println!("DMD not initialized.");
                    false
                }
            }
        },
        Err(e) => {
            println!("Could not lock DMD. Error={:?}", e);
            false
        }
    }
}

#[no_mangle]
fn dmd_rx_keyboard(c: uint8_t) -> bool {
    match DMD_STATE.lock() {
        Ok(mut dmd_guard) => {
            match &mut dmd_guard.dmd {
                Some(dmd) => {
                    match dmd.rx_keyboard(c) {
                        Ok(()) => {
                            println!("[DMD] Received character 0x{:02x}", c);
                            true
                        },
                        Err(DuartError::ReceiverNotReady) => {
                            false
                        }
                    }
                }
                None => {
                    println!("DMD not initialized.");
                    false
                }
            }
        },
        Err(e) => {
            println!("Could not lock DMD. Error={:?}", e);
            false
        }
    }
}

#[no_mangle]
fn dmd_mouse_move(x: uint16_t, y: uint16_t) -> bool {
    match DMD_STATE.lock() {
        Ok(mut dmd_guard) => {
            match &mut dmd_guard.dmd {
                Some(dmd) => {
                    dmd.mouse_move(x, y);
                    true
                }
                None => {
                    println!("DMD not initialized.");
                    false
                }
            }
        },
        Err(e) => {
            println!("Could not lock DMD. Error={:?}", e);
            false
        }
    }
}

#[no_mangle]
fn dmd_mouse_down(button: uint8_t) -> bool {
    match DMD_STATE.lock() {
        Ok(mut dmd_guard) => {
            match &mut dmd_guard.dmd {
                Some(dmd) => {
                    dmd.mouse_down(button);
                    true
                }
                None => {
                    println!("DMD not initialized.");
                    false
                }
            }
        },
        Err(e) => {
            println!("Could not lock DMD. Error={:?}", e);
            false
        }
    }
}

#[no_mangle]
fn dmd_mouse_up(button: uint8_t) -> bool {
    match DMD_STATE.lock() {
        Ok(mut dmd_guard) => {
            match &mut dmd_guard.dmd {
                Some(dmd) => {
                    dmd.mouse_up(button as u8);
                    true
                }
                None => {
                    println!("DMD not initialized.");
                    false
                }
            }
        },
        Err(e) => {
            println!("Could not lock DMD. Error={:?}", e);
            false
        }
    }
}

// #[no_mangle]
// fn tx_poll(mut cx: FunctionContext) -> JsResult<JsObject> {
//     let o = JsObject::new(&mut cx);
//     match DMD_STATE.lock() {
//         Ok(mut dmd_guard) => {
//             match &mut dmd_guard.dmd {
//                 Some(dmd) => {
//                     match dmd.tx_poll() {
//                         Some(c) => {
//                             let success = cx.boolean(true);
//                             let char = cx.number(c);
//                             o.set(&mut cx, "success", success)?;
//                             o.set(&mut cx, "char", char)?;
//                         },
//                         None => {
//                             let success = cx.boolean(false);
//                             let char = cx.number(0);
//                             o.set(&mut cx, "success", success)?;
//                             o.set(&mut cx, "char", char)?;
//                         }
//                     }
//                 }
//                 None => {
//                     let success = cx.boolean(false);
//                     let char = cx.number(0);
//                     o.set(&mut cx, "success", success)?;
//                     o.set(&mut cx, "char", char)?;
//                 }
//             }
//         },
//         Err(_) => {
//             let success = cx.boolean(false);
//             let char = cx.number(0);
//             o.set(&mut cx, "success", success)?;
//             o.set(&mut cx, "char", char)?;
//         }
//     }

//     Ok(o)
// }

#[no_mangle]
fn test_function() -> int8_t {
    return 0x5a;
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
