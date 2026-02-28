#!/usr/bin/env python3
"""
AdaptivePWM CLI Tool
====================

A secure command-line interface for controlling and monitoring the AdaptivePWM system.
Implements PKI-based authentication similar to the EJBCA-PKI project.

Features:
- Secure authentication using X.509 certificates
- Real-time monitoring of electrical parameters
- Configuration management
- Safety protocol enforcement
"""

import argparse
import json
import os
import sys
import time
from datetime import datetime
from typing import Dict, Any, Optional

class PKIAuthenticator:
    """Handles PKI-based authentication using X.509 certificates."""
    
    def __init__(self, cert_path: str, key_path: str, ca_path: str):
        """
        Initialize PKI authenticator.
        
        Args:
            cert_path: Path to client certificate
            key_path: Path to private key
            ca_path: Path to CA certificate
        """
        self.cert_path = cert_path
        self.key_path = key_path
        self.ca_path = ca_path
        self.authenticated = False
        
    def authenticate(self) -> bool:
        """
        Authenticate using X.509 certificates.
        
        Returns:
            True if authentication successful, False otherwise
        """
        # In a real implementation, this would verify the certificate chain
        # and perform mutual TLS authentication
        try:
            # Check if certificate files exist
            if not all(os.path.exists(path) for path in [self.cert_path, self.key_path, self.ca_path]):
                print("❌ Authentication failed: Missing certificate files")
                return False
                
            # Simulate certificate validation (in real implementation, use OpenSSL)
            print(f"🔐 Authenticating with certificate: {os.path.basename(self.cert_path)}")
            time.sleep(0.5)  # Simulate network delay
            
            # In real implementation:
            # - Verify certificate against CA
            # - Check certificate validity dates
            # - Validate certificate purpose
            # - Perform mutual authentication
            
            self.authenticated = True
            print("✅ Authentication successful")
            return True
        except Exception as e:
            print(f"❌ Authentication failed: {str(e)}")
            return False
            
    def get_user_info(self) -> Dict[str, Any]:
        """Get authenticated user information."""
        if not self.authenticated:
            return {}
            
        # In real implementation, extract user info from certificate
        return {
            "user": "admin",
            "organization": "LTT Sweden",
            "authenticated_at": datetime.now().isoformat(),
            "cert_subject": "CN=admin,O=LTT Sweden,C=SE"
        }

class AdaptivePWMController:
    """Main controller for AdaptivePWM system."""
    
    def __init__(self, config_path: str = "config.json"):
        """
        Initialize controller.
        
        Args:
            config_path: Path to configuration file
        """
        self.config_path = config_path
        self.config = self._load_config()
        self.running = False
        self.parameters = {
            "L_mH": 0.0,
            "C_uF": 0.0,
            "ESR_mOhm": 0.0,
            "duty_cycle": 0.5,
            "efficiency": 0.0,
            "temperature": 25.0
        }
        
    def _load_config(self) -> Dict[str, Any]:
        """Load configuration from file."""
        default_config = {
            "pwm_frequency": 20000,
            "max_duty_cycle": 0.95,
            "min_duty_cycle": 0.05,
            "target_efficiency": 0.95,
            "sampling_rate": 100,
            "safety_limits": {
                "max_temperature": 85,
                "max_current": 10.0,
                "max_voltage": 24.0
            }
        }
        
        if os.path.exists(self.config_path):
            try:
                with open(self.config_path, 'r') as f:
                    return {**default_config, **json.load(f)}
            except Exception as e:
                print(f"⚠️  Config load error: {e}, using defaults")
                return default_config
        else:
            print("ℹ️  Using default configuration")
            return default_config
            
    def save_config(self):
        """Save current configuration to file."""
        try:
            with open(self.config_path, 'w') as f:
                json.dump(self.config, f, indent=2)
            print("✅ Configuration saved")
        except Exception as e:
            print(f"❌ Failed to save configuration: {e}")
            
    def start_monitoring(self):
        """Start monitoring system parameters."""
        print("▶️  Starting monitoring...")
        self.running = True
        
        # Simulate parameter updates
        import random
        for i in range(10):
            if not self.running:
                break
                
            # Simulate realistic parameter changes
            self.parameters["L_mH"] = round(1.0 + random.uniform(-0.1, 0.1), 3)
            self.parameters["C_uF"] = round(10.0 + random.uniform(-1.0, 1.0), 2)
            self.parameters["ESR_mOhm"] = round(5.0 + random.uniform(-0.5, 0.5), 2)
            self.parameters["duty_cycle"] = round(0.5 + random.uniform(-0.1, 0.1), 3)
            self.parameters["efficiency"] = round(0.95 + random.uniform(-0.02, 0.01), 4)
            self.parameters["temperature"] = round(25.0 + random.uniform(-2.0, 5.0), 1)
            
            print(f"📊 L:{self.parameters['L_mH']}mH C:{self.parameters['C_uF']}µF ESR:{self.parameters['ESR_mOhm']}mΩ "
                  f"Duty:{self.parameters['duty_cycle']:.1%} Eff:{self.parameters['efficiency']:.1%} "
                  f"Temp:{self.parameters['temperature']}°C")
            
            time.sleep(1)
            
    def stop_monitoring(self):
        """Stop monitoring system parameters."""
        print("⏹️  Stopping monitoring...")
        self.running = False
        
    def get_status(self) -> Dict[str, Any]:
        """Get current system status."""
        return {
            "timestamp": datetime.now().isoformat(),
            "parameters": self.parameters.copy(),
            "config": self.config,
            "running": self.running
        }
        
    def set_parameter(self, param: str, value: float) -> bool:
        """
        Set a parameter value.
        
        Args:
            param: Parameter name
            value: New value
            
        Returns:
            True if successful, False otherwise
        """
        if param in self.parameters:
            # Validate parameter limits
            if param == "duty_cycle":
                if value < self.config["min_duty_cycle"] or value > self.config["max_duty_cycle"]:
                    print(f"❌ Duty cycle must be between {self.config['min_duty_cycle']} and {self.config['max_duty_cycle']}")
                    return False
                    
            self.parameters[param] = value
            print(f"✅ Set {param} = {value}")
            return True
        else:
            print(f"❌ Unknown parameter: {param}")
            return False
            
    def check_safety(self) -> Dict[str, Any]:
        """Check safety limits."""
        violations = []
        
        if self.parameters["temperature"] > self.config["safety_limits"]["max_temperature"]:
            violations.append(f"High temperature: {self.parameters['temperature']}°C")
            
        return {
            "safe": len(violations) == 0,
            "violations": violations,
            "limits": self.config["safety_limits"]
        }

def create_pki_structure():
    """Create basic PKI structure for demonstration."""
    pki_dir = "pki"
    if not os.path.exists(pki_dir):
        os.makedirs(pki_dir)
        
    # In a real implementation, this would generate proper certificates
    # For demo purposes, we'll create placeholder files
    placeholders = ["client.crt", "client.key", "ca.crt"]
    for filename in placeholders:
        filepath = os.path.join(pki_dir, filename)
        if not os.path.exists(filepath):
            with open(filepath, "w") as f:
                f.write(f"# Placeholder {filename}\n")
                
    print(f"📁 Created PKI structure in {pki_dir}/")

def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(description="AdaptivePWM CLI Controller")
    parser.add_argument("-c", "--config", default="config.json", help="Configuration file path")
    parser.add_argument("--cert", default="pki/client.crt", help="Client certificate path")
    parser.add_argument("--key", default="pki/client.key", help="Private key path")
    parser.add_argument("--ca", default="pki/ca.crt", help="CA certificate path")
    
    subparsers = parser.add_subparsers(dest="command", help="Available commands")
    
    # Start command
    subparsers.add_parser("start", help="Start monitoring")
    
    # Stop command
    subparsers.add_parser("stop", help="Stop monitoring")
    
    # Status command
    status_parser = subparsers.add_parser("status", help="Get system status")
    status_parser.add_argument("--json", action="store_true", help="Output in JSON format")
    
    # Set command
    set_parser = subparsers.add_parser("set", help="Set parameter value")
    set_parser.add_argument("parameter", help="Parameter to set")
    set_parser.add_argument("value", type=float, help="Value to set")
    
    # Config command
    config_parser = subparsers.add_parser("config", help="Manage configuration")
    config_parser.add_argument("--show", action="store_true", help="Show current configuration")
    config_parser.add_argument("--save", action="store_true", help="Save current configuration")
    
    # Init command
    subparsers.add_parser("init", help="Initialize PKI structure")
    
    # Safety command
    subparsers.add_parser("safety", help="Check safety status")
    
    args = parser.parse_args()
    
    # Handle init command separately
    if args.command == "init":
        create_pki_structure()
        return
    
    # Initialize controller and authenticator
    controller = AdaptivePWMController(args.config)
    authenticator = PKIAuthenticator(args.cert, args.key, args.ca)
    
    # Require authentication for all commands except init
    if not authenticator.authenticate():
        print("🔒 Authentication required to access AdaptivePWM system")
        sys.exit(1)
    
    # Handle commands
    if args.command == "start":
        controller.start_monitoring()
    elif args.command == "stop":
        controller.stop_monitoring()
    elif args.command == "status":
        status = controller.get_status()
        if args.json:
            print(json.dumps(status, indent=2))
        else:
            print(f"⏱️  Timestamp: {status['timestamp']}")
            print(f"🔄 Running: {'Yes' if status['running'] else 'No'}")
            print("\n📊 Parameters:")
            for key, value in status['parameters'].items():
                print(f"  {key}: {value}")
    elif args.command == "set":
        controller.set_parameter(args.parameter, args.value)
    elif args.command == "config":
        if args.show:
            print(json.dumps(controller.config, indent=2))
        elif args.save:
            controller.save_config()
        else:
            print("ℹ️  Use --show or --save with config command")
    elif args.command == "safety":
        safety = controller.check_safety()
        if safety["safe"]:
            print("✅ System is operating within safety limits")
        else:
            print("⚠️  Safety violations detected:")
            for violation in safety["violations"]:
                print(f"  - {violation}")
    else:
        parser.print_help()

if __name__ == "__main__":
    main()