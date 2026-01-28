import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/phase_service.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final service = Provider.of<PhaseService>(context, listen: false);
      if (!service.isConnected) {
        _showConnectionDialog(context);
      }
    });
  }

  void _showConnectionDialog(BuildContext context) {
    final service = Provider.of<PhaseService>(context, listen: false);
    final ipController = TextEditingController(text: service.serverIP);

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Connect to Device'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: ipController,
              decoration: const InputDecoration(
                labelText: 'Device IP Address',
                hintText: '192.168.1.100',
                border: OutlineInputBorder(),
              ),
              keyboardType: TextInputType.number,
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () {
              service.setServerIP(ipController.text);
              service.connect().then((_) {
                Navigator.pop(context);
                if (!service.isConnected) {
                  ScaffoldMessenger.of(context).showSnackBar(
                    SnackBar(
                      content: Text(service.errorMessage ?? 'Connection failed'),
                      backgroundColor: Colors.red,
                    ),
                  );
                }
              });
            },
            child: const Text('Connect'),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Best Phase Detector'),
        backgroundColor: Theme.of(context).colorScheme.inversePrimary,
        actions: [
          Consumer<PhaseService>(
            builder: (context, service, _) => IconButton(
              icon: Icon(service.isConnected ? Icons.wifi : Icons.wifi_off),
              onPressed: () {
                if (service.isConnected) {
                  service.disconnect();
                } else {
                  _showConnectionDialog(context);
                }
              },
            ),
          ),
        ],
      ),
      body: Consumer<PhaseService>(
        builder: (context, service, _) {
          if (service.isLoading) {
            return const Center(child: CircularProgressIndicator());
          }

          if (!service.isConnected) {
            return Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const Icon(Icons.wifi_off, size: 64, color: Colors.grey),
                  const SizedBox(height: 16),
                  const Text(
                    'Not Connected',
                    style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold),
                  ),
                  const SizedBox(height: 8),
                  Text(
                    service.errorMessage ?? 'Connect to device to continue',
                    textAlign: TextAlign.center,
                    style: const TextStyle(color: Colors.grey),
                  ),
                  const SizedBox(height: 24),
                  ElevatedButton.icon(
                    onPressed: () => _showConnectionDialog(context),
                    icon: const Icon(Icons.wifi),
                    label: const Text('Connect'),
                  ),
                ],
              ),
            );
          }

          if (service.status == null) {
            return const Center(child: Text('No data available'));
          }

          final status = service.status!;
          return RefreshIndicator(
            onRefresh: () => service.fetchStatus(),
            child: SingleChildScrollView(
              physics: const AlwaysScrollableScrollPhysics(),
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  _buildStatusCard(status, service),
                  const SizedBox(height: 16),
                  _buildModeSelector(status, service),
                  const SizedBox(height: 16),
                  _buildPhaseCards(status, service),
                ],
              ),
            ),
          );
        },
      ),
      floatingActionButton: Consumer<PhaseService>(
        builder: (context, service, _) => FloatingActionButton(
          onPressed: service.isConnected ? () => service.fetchStatus() : null,
          child: const Icon(Icons.refresh),
        ),
      ),
    );
  }

  Widget _buildStatusCard(SystemStatus status, PhaseService service) {
    return Card(
      elevation: 4,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text(
                  'System Status',
                  style: Theme.of(context).textTheme.titleLarge,
                ),
                Container(
                  padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
                  decoration: BoxDecoration(
                    color: status.mode == 'automatic'
                        ? Colors.green
                        : Colors.orange,
                    borderRadius: BorderRadius.circular(20),
                  ),
                  child: Text(
                    status.mode.toUpperCase(),
                    style: const TextStyle(
                      color: Colors.white,
                      fontWeight: FontWeight.bold,
                      fontSize: 12,
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            Row(
              children: [
                const Icon(Icons.flash_on, size: 20),
                const SizedBox(width: 8),
                Text(
                  'Best Phase: ${status.bestPhase >= 0 ? status.phases[status.bestPhase].name : "N/A"}',
                  style: Theme.of(context).textTheme.bodyLarge,
                ),
              ],
            ),
            if (status.selectedPhase >= 0) ...[
              const SizedBox(height: 8),
              Row(
                children: [
                  const Icon(Icons.check_circle, size: 20, color: Colors.green),
                  const SizedBox(width: 8),
                  Text(
                    'Active: ${status.phases[status.selectedPhase].name}',
                    style: Theme.of(context).textTheme.bodyLarge,
                  ),
                ],
              ),
            ],
          ],
        ),
      ),
    );
  }

  Widget _buildModeSelector(SystemStatus status, PhaseService service) {
    return Card(
      elevation: 4,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Operation Mode',
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 12),
            Row(
              children: [
                Expanded(
                  child: ElevatedButton(
                    onPressed: status.mode == 'automatic'
                        ? null
                        : () => service.setMode('auto'),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: status.mode == 'automatic'
                          ? Colors.green
                          : Colors.grey,
                      foregroundColor: Colors.white,
                    ),
                    child: const Text('Automatic'),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: ElevatedButton(
                    onPressed: status.mode == 'manual'
                        ? null
                        : () => service.setMode('manual'),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: status.mode == 'manual'
                          ? Colors.orange
                          : Colors.grey,
                      foregroundColor: Colors.white,
                    ),
                    child: const Text('Manual'),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildPhaseCards(SystemStatus status, PhaseService service) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(
          'Phase Voltages',
          style: Theme.of(context).textTheme.titleLarge,
        ),
        const SizedBox(height: 12),
        ...status.phases.asMap().entries.map((entry) {
          final index = entry.key;
          final phase = entry.value;
          return _buildPhaseCard(phase, index, status, service);
        }),
      ],
    );
  }

  Widget _buildPhaseCard(
    PhaseData phase,
    int index,
    SystemStatus status,
    PhaseService service,
  ) {
    final isActive = phase.isActive;
    final isBest = status.bestPhase == index;
    final isManualMode = status.mode == 'manual';

    return Card(
      elevation: isActive ? 6 : 2,
      color: isActive ? Colors.green.shade50 : null,
      margin: const EdgeInsets.only(bottom: 12),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Row(
                  children: [
                    Text(
                      phase.name,
                      style: Theme.of(context).textTheme.titleLarge?.copyWith(
                            fontWeight: FontWeight.bold,
                            color: isActive ? Colors.green.shade900 : null,
                          ),
                    ),
                    if (isActive) ...[
                      const SizedBox(width: 8),
                      const Icon(Icons.check_circle, color: Colors.green),
                    ],
                    if (isBest && !isActive) ...[
                      const SizedBox(width: 8),
                      Container(
                        padding: const EdgeInsets.symmetric(
                          horizontal: 8,
                          vertical: 4,
                        ),
                        decoration: BoxDecoration(
                          color: Colors.blue,
                          borderRadius: BorderRadius.circular(12),
                        ),
                        child: const Text(
                          'BEST',
                          style: TextStyle(
                            color: Colors.white,
                            fontSize: 10,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                      ),
                    ],
                  ],
                ),
              ],
            ),
            const SizedBox(height: 12),
            Row(
              children: [
                Expanded(
                  child: _buildVoltageInfo(
                    'Current',
                    phase.voltage,
                    Icons.bolt,
                  ),
                ),
                Expanded(
                  child: _buildVoltageInfo(
                    'Average',
                    phase.avgVoltage,
                    Icons.trending_up,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),
            Row(
              children: [
                Expanded(
                  child: _buildVoltageInfo(
                    'Min',
                    phase.minVoltage,
                    Icons.arrow_downward,
                  ),
                ),
                Expanded(
                  child: _buildVoltageInfo(
                    'Max',
                    phase.maxVoltage,
                    Icons.arrow_upward,
                  ),
                ),
              ],
            ),
            if (isManualMode) ...[
              const SizedBox(height: 16),
              SizedBox(
                width: double.infinity,
                child: ElevatedButton(
                  onPressed: isActive
                      ? null
                      : () {
                          service.setPhase(index).then((success) {
                            if (success) {
                              ScaffoldMessenger.of(context).showSnackBar(
                                SnackBar(
                                  content: Text('Switched to ${phase.name}'),
                                  backgroundColor: Colors.green,
                                ),
                              );
                            } else {
                              ScaffoldMessenger.of(context).showSnackBar(
                                const SnackBar(
                                  content: Text('Failed to switch phase'),
                                  backgroundColor: Colors.red,
                                ),
                              );
                            }
                          });
                        },
                  style: ElevatedButton.styleFrom(
                    backgroundColor: isActive ? Colors.grey : Colors.blue,
                    foregroundColor: Colors.white,
                  ),
                  child: Text(isActive ? 'Active' : 'Select Phase'),
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }

  Widget _buildVoltageInfo(String label, double voltage, IconData icon) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Icon(icon, size: 16, color: Colors.grey.shade600),
            const SizedBox(width: 4),
            Text(
              label,
              style: TextStyle(
                fontSize: 12,
                color: Colors.grey.shade600,
              ),
            ),
          ],
        ),
        const SizedBox(height: 4),
        Text(
          '${voltage.toStringAsFixed(1)} V',
          style: const TextStyle(
            fontSize: 18,
            fontWeight: FontWeight.bold,
          ),
        ),
      ],
    );
  }
}

