import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;
import '../models/phase_data.dart';

class PhaseService extends ChangeNotifier {
  String _serverIP = '192.168.1.100'; // Default IP, user can change
  bool _isConnected = false;
  bool _isLoading = false;
  SystemStatus? _status;
  String? _errorMessage;

  String get serverIP => _serverIP;
  bool get isConnected => _isConnected;
  bool get isLoading => _isLoading;
  SystemStatus? get status => _status;
  String? get errorMessage => _errorMessage;

  void setServerIP(String ip) {
    _serverIP = ip;
    notifyListeners();
  }

  Future<void> connect() async {
    _isLoading = true;
    _errorMessage = null;
    notifyListeners();

    try {
      final response = await http
          .get(Uri.parse('http://$_serverIP/api/status'))
          .timeout(const Duration(seconds: 5));

      if (response.statusCode == 200) {
        _status = SystemStatus.fromJson(json.decode(response.body));
        _isConnected = true;
        _errorMessage = null;
      } else {
        _isConnected = false;
        _errorMessage = 'Failed to connect: ${response.statusCode}';
      }
    } catch (e) {
      _isConnected = false;
      _errorMessage = 'Connection error: ${e.toString()}';
    } finally {
      _isLoading = false;
      notifyListeners();
    }
  }

  Future<void> fetchStatus() async {
    if (!_isConnected) return;

    try {
      final response = await http
          .get(Uri.parse('http://$_serverIP/api/status'))
          .timeout(const Duration(seconds: 3));

      if (response.statusCode == 200) {
        _status = SystemStatus.fromJson(json.decode(response.body));
        _errorMessage = null;
        notifyListeners();
      }
    } catch (e) {
      _errorMessage = 'Failed to fetch status: ${e.toString()}';
      notifyListeners();
    }
  }

  Future<bool> setPhase(int phaseIndex) async {
    if (!_isConnected) return false;

    try {
      final response = await http.post(
        Uri.parse('http://$_serverIP/api/setPhase'),
        headers: {'Content-Type': 'application/json'},
        body: json.encode({'phase': phaseIndex}),
      ).timeout(const Duration(seconds: 5));

      if (response.statusCode == 200) {
        await fetchStatus();
        return true;
      }
      return false;
    } catch (e) {
      _errorMessage = 'Failed to set phase: ${e.toString()}';
      notifyListeners();
      return false;
    }
  }

  Future<bool> setMode(String mode) async {
    if (!_isConnected) return false;

    try {
      final response = await http.post(
        Uri.parse('http://$_serverIP/api/setMode'),
        headers: {'Content-Type': 'application/json'},
        body: json.encode({'mode': mode}),
      ).timeout(const Duration(seconds: 5));

      if (response.statusCode == 200) {
        await fetchStatus();
        return true;
      }
      return false;
    } catch (e) {
      _errorMessage = 'Failed to set mode: ${e.toString()}';
      notifyListeners();
      return false;
    }
  }

  void disconnect() {
    _isConnected = false;
    _status = null;
    notifyListeners();
  }
}

