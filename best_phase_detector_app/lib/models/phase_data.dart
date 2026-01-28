class PhaseData {
  final String name;
  final double voltage;
  final double avgVoltage;
  final double minVoltage;
  final double maxVoltage;
  final bool isActive;

  PhaseData({
    required this.name,
    required this.voltage,
    required this.avgVoltage,
    required this.minVoltage,
    required this.maxVoltage,
    required this.isActive,
  });

  factory PhaseData.fromJson(Map<String, dynamic> json) {
    return PhaseData(
      name: json['name'] ?? '',
      voltage: (json['voltage'] ?? 0.0).toDouble(),
      avgVoltage: (json['avgVoltage'] ?? 0.0).toDouble(),
      minVoltage: (json['minVoltage'] ?? 0.0).toDouble(),
      maxVoltage: (json['maxVoltage'] ?? 0.0).toDouble(),
      isActive: json['isActive'] ?? false,
    );
  }
}

class SystemStatus {
  final String mode;
  final int bestPhase;
  final int selectedPhase;
  final List<PhaseData> phases;

  SystemStatus({
    required this.mode,
    required this.bestPhase,
    required this.selectedPhase,
    required this.phases,
  });

  factory SystemStatus.fromJson(Map<String, dynamic> json) {
    return SystemStatus(
      mode: json['mode'] ?? 'automatic',
      bestPhase: json['bestPhase'] ?? -1,
      selectedPhase: json['selectedPhase'] ?? -1,
      phases: (json['phases'] as List<dynamic>?)
              ?.map((p) => PhaseData.fromJson(p as Map<String, dynamic>))
              .toList() ??
          [],
    );
  }
}

