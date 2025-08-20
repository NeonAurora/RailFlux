#include "AspectPropagationService.h"
#include "InterlockingRuleEngine.h"
#include "../database/DatabaseManager.h"
#include <QElapsedTimer>
#include <QDebug>
#include <QTimer>
#include <algorithm>
#include <queue>

using namespace RailFlux::Interlocking;

AspectPropagationService::AspectPropagationService(
    DatabaseManager* dbManager,
    InterlockingRuleEngine* ruleEngine,
    QObject* parent)
    : QObject(parent)
    , m_dbManager(dbManager)
    , m_ruleEngine(ruleEngine)
{
    Q_ASSERT(dbManager);
    Q_ASSERT(ruleEngine);
    
    qDebug() << "🔧 [ASPECT_PROPAGATION] Service created";
    
    // Initialize with default configuration
    loadDefaultConfiguration();
}

AspectPropagationService::~AspectPropagationService()
{
    qDebug() << "🔧 [ASPECT_PROPAGATION] Service destroyed";
}

void AspectPropagationService::initialize()
{
    if (m_isInitialized) {
        qWarning() << "⚠️ [ASPECT_PROPAGATION] Already initialized";
        return;
    }

    qDebug() << "🚀 [ASPECT_PROPAGATION] Initializing service...";

    // Verify dependencies
    if (!m_dbManager || !m_dbManager->isConnected()) {
        qCritical() << "❌ [ASPECT_PROPAGATION] Database manager not available";
        return;
    }

    if (!m_ruleEngine) {
        qCritical() << "❌ [ASPECT_PROPAGATION] Interlocking rule engine not available";
        return;
    }

    m_isOperational = true;
    m_isInitialized = true;

    // Set up performance monitoring timer
    if (m_enablePerformanceMonitoring) {
        QTimer* performanceTimer = new QTimer(this);
        connect(performanceTimer, &QTimer::timeout,
                this, &AspectPropagationService::performPerformanceCheck);
        performanceTimer->start(10000); // Check every 10 seconds
    }

    qDebug() << "✅ [ASPECT_PROPAGATION] Service initialized successfully";
    emit operationalStateChanged();
}

void AspectPropagationService::loadDefaultConfiguration()
{
    qDebug() << "📋 [ASPECT_PROPAGATION] Loading default configuration...";

    // Set default destination constraints (destinations typically show RED)
    m_destinationConstraints["HOME"] = "RED";
    m_destinationConstraints["STARTER"] = "RED";
    m_destinationConstraints["OUTER"] = "RED";
    // Advanced Starters may show proceed if track circuits clear
    m_destinationConstraints["ADVANCED_STARTER"] = "GREEN_OR_RED";

    // Set aspect selection priorities (most permissive first for efficiency)
    m_aspectPriorities["HOME"] = {"GREEN", "YELLOW", "RED"};
    m_aspectPriorities["STARTER"] = {"GREEN", "YELLOW", "RED"};
    m_aspectPriorities["OUTER"] = {"GREEN", "YELLOW", "RED"};
    m_aspectPriorities["ADVANCED_STARTER"] = {"GREEN", "YELLOW", "RED"};

    qDebug() << "📋 [ASPECT_PROPAGATION] Configuration loaded";
}

QVariantMap AspectPropagationService::propagateAspects(
    const QString& sourceSignalId,
    const QString& destinationSignalId,
    const QVariantMap& pointMachinePositions)
{
    return propagateAspectsAdvanced(sourceSignalId, destinationSignalId, 
                                  pointMachinePositions, QVariantMap());
}

QVariantMap AspectPropagationService::propagateAspectsAdvanced(
    const QString& sourceSignalId,
    const QString& destinationSignalId,
    const QVariantMap& pointMachinePositions,
    const QVariantMap& options)
{
    if (!m_isOperational) {
        qWarning() << "⚠️ [ASPECT_PROPAGATION] Service not operational";
        QVariantMap errorResult;
        errorResult["success"] = false;
        errorResult["error"] = "Aspect propagation service not operational";
        errorResult["errorCode"] = "SERVICE_NOT_OPERATIONAL";
        return errorResult;
    }

    AspectPropagationResult result = propagateAspectsInternal(
        sourceSignalId, destinationSignalId, pointMachinePositions, options);
    
    return aspectPropagationResultToVariantMap(result);
}

AspectPropagationResult AspectPropagationService::propagateAspectsInternal(
    const QString& sourceSignalId,
    const QString& destinationSignalId,
    const QVariantMap& pointMachinePositions,
    const QVariantMap& options)
{
    QElapsedTimer timer;
    timer.start();
    
    AspectPropagationResult result;
    m_totalPropagations++;

    qDebug() << "🔄 [ASPECT_PROPAGATION] Starting propagation:"
             << sourceSignalId << "→" << destinationSignalId;

    try {
        // 1. Validate the propagation request
        QVariantMap validation = validatePropagationRequestInternal(sourceSignalId, destinationSignalId);
        if (!validation["success"].toBool()) {
            result.errorMessage = validation["error"].toString();
            result.errorCode = validation["errorCode"].toString();
            result.processingTimeMs = timer.elapsed();
            qWarning() << "❌ [ASPECT_PROPAGATION] Validation failed:" << result.errorMessage;
            return result;
        }

        // 2. Build the complete control graph starting from source
        QVariantMap fullGraph = buildControlGraphInternal(sourceSignalId);
        result.graphSize = fullGraph["nodes"].toMap().size();
        
        emit graphConstructed(result.graphSize, fullGraph["edges"].toList().size());
        
        qDebug() << "📊 [ASPECT_PROPAGATION] Full control graph:"
                 << result.graphSize << "nodes,"
                 << fullGraph["edges"].toList().size() << "edges";

        // 3. Prune graph to focus on source→destination control path
        QVariantMap prunedGraph = pruneGraphForDestinationInternal(fullGraph, destinationSignalId);
        result.prunedGraphSize = prunedGraph["nodes"].toMap().size();
        
        emit graphPruned(result.graphSize, result.prunedGraphSize);
        
        qDebug() << "✂️ [ASPECT_PROPAGATION] Pruned to" << result.prunedGraphSize 
                 << "nodes for relevant control path";

        // 4. Create dependency-ordered processing sequence
        QVector<ControlNode> orderedNodes = createDependencyOrder(prunedGraph);
        
        qDebug() << "📋 [ASPECT_PROPAGATION] Processing order:";
        for (int i = 0; i < orderedNodes.size(); ++i) {
            qDebug() << "   " << (i+1) << "." << orderedNodes[i].signalId 
                     << "(" << orderedNodes[i].signalType << ")"
                     << (orderedNodes[i].isIndependent ? "[INDEPENDENT]" : "");
        }

        // 5. Forward propagate aspects through dependency chain
        QVariantMap aspectSelections = selectOptimalAspects(
            orderedNodes, destinationSignalId, pointMachinePositions, options);

        if (!aspectSelections["success"].toBool()) {
            result.errorMessage = aspectSelections["error"].toString();
            result.errorCode = aspectSelections.value("errorCode", "PROPAGATION_FAILED").toString();
            result.processingTimeMs = timer.elapsed();
            return result;
        }

        // 6. Build successful result
        result.success = true;
        result.signalAspects = aspectSelections["aspects"].toMap();
        result.pointMachines = aspectSelections["pointMachines"].toMap();
        result.decisionReasons = aspectSelections["reasons"].toMap();
        result.processedSignals = aspectSelections["processOrder"].toStringList();
        
        // Extract pruned signals for analysis
        QSet<QString> allSignals(fullGraph["nodes"].toMap().keys().begin(), fullGraph["nodes"].toMap().keys().end());
        QSet<QString> relevantSignals(prunedGraph["nodes"].toMap().keys().begin(), prunedGraph["nodes"].toMap().keys().end());
        result.prunedSignals = (allSignals - relevantSignals).values();

        m_successfulPropagations++;
        
        qDebug() << "✅ [ASPECT_PROPAGATION] Success! Selected aspects:";
        for (auto it = result.signalAspects.begin(); it != result.signalAspects.end(); ++it) {
            qDebug() << "   " << it.key() << "→" << it.value().toString();
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = QString("Propagation algorithm error: %1").arg(e.what());
        result.errorCode = "ALGORITHM_ERROR";
        qCritical() << "💥 [ASPECT_PROPAGATION] Exception:" << e.what();
    }

    result.processingTimeMs = timer.elapsed();
    
    // Record performance metrics
    recordProcessingTime("full_propagation", result.processingTimeMs);
    recordPropagationResult(result);

    // Performance monitoring
    if (result.processingTimeMs > TARGET_PROCESSING_TIME_MS) {
        qWarning() << "⚠️ [ASPECT_PROPAGATION] Slow processing:" 
                   << result.processingTimeMs << "ms";
        emit performanceWarning("propagation", result.processingTimeMs, TARGET_PROCESSING_TIME_MS);
    }

    emit propagationCompleted(sourceSignalId, destinationSignalId, result.success);
    
    if (!result.success) {
        emit aspectPropagationFailed(sourceSignalId, destinationSignalId, result.errorMessage);
    }
    
    return result;
}

QVariantMap AspectPropagationService::buildControlGraph(const QString& sourceSignalId)
{
    if (!m_isOperational) {
        QVariantMap errorResult;
        errorResult["success"] = false;
        errorResult["error"] = "Service not operational";
        return errorResult;
    }
    
    return buildControlGraphInternal(sourceSignalId);
}

QVariantMap AspectPropagationService::buildControlGraphInternal(const QString& sourceSignalId)
{
    QElapsedTimer timer;
    timer.start();
    
    QHash<QString, ControlNode> nodes;
    QVector<ControlEdge> edges;
    QSet<QString> visited;

    qDebug() << "🏗️ [GRAPH_BUILD] Starting from source:" << sourceSignalId;

    try {
        // Recursively expand the control network
        expandControlNetwork(sourceSignalId, nodes, edges, visited);

        // Convert to QVariantMap for serialization/debugging
        QVariantMap result;
        QVariantMap nodeMap;
        QVariantList edgeList;

        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            const QString& signalId = it.key();
            const ControlNode& node = it.value();
            nodeMap[signalId] = controlNodeToVariantMap(node);
        }

        for (const auto& edge : edges) {
            edgeList.append(controlEdgeToVariantMap(edge));
        }

        result["success"] = true;
        result["nodes"] = nodeMap;
        result["edges"] = edgeList;
        result["processingTimeMs"] = timer.elapsed();
        
        qDebug() << "🏗️ [GRAPH_BUILD] Completed:" << nodes.size() << "nodes," << edges.size() << "edges";
        
        recordProcessingTime("graph_construction", timer.elapsed());
        
        return result;
        
    } catch (const std::exception& e) {
        qCritical() << "💥 [GRAPH_BUILD] Exception:" << e.what();
        
        QVariantMap errorResult;
        errorResult["success"] = false;
        errorResult["error"] = QString("Graph construction failed: %1").arg(e.what());
        errorResult["processingTimeMs"] = timer.elapsed();
        return errorResult;
    }
}

void AspectPropagationService::expandControlNetwork(
    const QString& signalId,
    QHash<QString, ControlNode>& nodes,
    QVector<ControlEdge>& edges,
    QSet<QString>& visited)
{
    if (visited.contains(signalId)) {
        return; // Avoid infinite recursion
    }
    
    if (nodes.size() >= m_maxGraphSize) {
        qWarning() << "⚠️ [EXPAND] Maximum graph size reached:" << m_maxGraphSize;
        return;
    }
    
    visited.insert(signalId);

    qDebug() << "🔍 [EXPAND] Processing signal:" << signalId;

    try {
        // Load signal control data
        ControlNode node = loadSignalControlData(signalId);
        if (node.signalId.isEmpty()) {
            qWarning() << "⚠️ [EXPAND] Signal not found:" << signalId;
            return;
        }
        
        nodes[signalId] = node;

        qDebug() << "   📝 Type:" << node.signalType 
                 << "Controlled by:" << node.controlledBy
                 << "Controls:" << node.controls
                 << "Independent:" << node.isIndependent;

        // Load control edges for this signal
        QVector<ControlEdge> signalEdges = loadControlEdges(signalId);
        edges.append(signalEdges);

        // Process controlling signals (upstream)
        for (const QString& controllingSignalId : node.controlledBy) {
            expandControlNetwork(controllingSignalId, nodes, edges, visited);
        }

        // Process controlled signals (downstream)  
        for (const QString& controlledSignalId : node.controls) {
            expandControlNetwork(controlledSignalId, nodes, edges, visited);
        }
        
    } catch (const std::exception& e) {
        qWarning() << "⚠️ [EXPAND] Error processing signal" << signalId << ":" << e.what();
    }
}

ControlNode AspectPropagationService::loadSignalControlData(const QString& signalId)
{
    // Check cache first
    if (m_signalDataCache.contains(signalId)) {
        QDateTime now = QDateTime::currentDateTime();
        if (m_lastCacheUpdate.secsTo(now) < CACHE_VALIDITY_SECONDS) {
            return m_signalDataCache[signalId];
        }
    }

    ControlNode node;
    
    // Get signal information from database
    QVariantMap signalData = m_dbManager->getSignalById(signalId);
    if (signalData.isEmpty()) {
        return node; // Return empty node
    }

    // Fill basic signal data
    node.signalId = signalId;
    node.signalType = signalData["type"].toString();
    node.possibleAspects = signalData["possibleAspects"].toStringList();
    node.locationRow = signalData["row"].toDouble();
    node.locationCol = signalData["col"].toDouble();
    
    // Get control relationships
    node.controlledBy = getControllingSignals(signalId);
    node.controls = getControlledSignals(signalId);
    node.isIndependent = isSignalIndependent(signalId);
    
    // Default control mode
    node.controlMode = "AND"; // All controllers must permit the aspect
    
    // Cache the result
    m_signalDataCache[signalId] = node;
    m_lastCacheUpdate = QDateTime::currentDateTime();
    
    return node;
}

QStringList AspectPropagationService::getControllingSignals(const QString& signalId)
{
    // Check cache first
    QString cacheKey = signalId + "_controlling";
    if (m_controlRelationshipCache.contains(cacheKey)) {
        return m_controlRelationshipCache[cacheKey];
    }

    QStringList controllingSignals;
    
    if (m_ruleEngine) {
        // Try to get from interlocking rule engine if it has this method
        // For now, implement basic logic based on signal types and positions
        
        QVariantMap signalData = m_dbManager->getSignalById(signalId);
        QString signalType = signalData["type"].toString();
        
        // Basic control hierarchy logic
        if (signalType == "HOME") {
            // Home signals are typically controlled by Starter signals
            controllingSignals = findSignalsByType("STARTER");
        } else if (signalType == "STARTER") {
            // Starter signals are typically controlled by Advanced Starter signals
            controllingSignals = findSignalsByType("ADVANCED_STARTER");
        }
        // OUTER and ADVANCED_STARTER signals are typically independent
    }
    
    // Cache the result
    m_controlRelationshipCache[cacheKey] = controllingSignals;
    
    return controllingSignals;
}

QStringList AspectPropagationService::getControlledSignals(const QString& signalId)
{
    // Check cache first
    QString cacheKey = signalId + "_controlled";
    if (m_controlRelationshipCache.contains(cacheKey)) {
        return m_controlRelationshipCache[cacheKey];
    }

    QStringList controlledSignals;
    
    if (m_ruleEngine) {
        QVariantMap signalData = m_dbManager->getSignalById(signalId);
        QString signalType = signalData["type"].toString();
        
        // Basic control hierarchy logic (reverse of controlling)
        if (signalType == "ADVANCED_STARTER") {
            // Advanced Starter signals typically control Starter signals
            controlledSignals = findSignalsByType("STARTER");
        } else if (signalType == "STARTER") {
            // Starter signals typically control Home signals
            controlledSignals = findSignalsByType("HOME");
        }
        // HOME and OUTER signals typically don't control other signals
    }
    
    // Cache the result
    m_controlRelationshipCache[cacheKey] = controlledSignals;
    
    return controlledSignals;
}

QStringList AspectPropagationService::findSignalsByType(const QString& signalType)
{
    QStringList signalList;
    
    // Get all signals of the specified type from database
    QVariantList allSignals = m_dbManager->getAllSignalsList();
    
    for (const QVariant& signalVariant : allSignals) {
        QVariantMap signal = signalVariant.toMap();
        if (signal["type"].toString() == signalType) {
            signalList.append(signal["id"].toString());
        }
    }
    
    return signalList;
}

bool AspectPropagationService::isSignalIndependent(const QString& signalId)
{
    QVariantMap signalData = m_dbManager->getSignalById(signalId);
    QString signalType = signalData["type"].toString();
    
    // Independent signals can set their aspect without considering other signals
    // Typically OUTER and ADVANCED_STARTER signals are independent
    return signalType == "OUTER" || signalType == "ADVANCED_STARTER";
}

QVector<ControlEdge> AspectPropagationService::loadControlEdges(const QString& signalId)
{
    QVector<ControlEdge> edges;
    
    // Get control relationships and create edges
    QStringList controlling = getControllingSignals(signalId);
    
    for (const QString& controllingSignalId : controlling) {
        ControlEdge edge;
        edge.fromSignalId = controllingSignalId;
        edge.toSignalId = signalId;
        edge.whenAspect = "GREEN"; // Simplified - in real system would come from rules
        edge.allowedAspects = QStringList{"GREEN", "YELLOW", "RED"}; // Simplified
        edge.ruleId = QString("rule_%1_%2").arg(controllingSignalId, signalId);
        
        edges.append(edge);
    }
    
    return edges;
}

double AspectPropagationService::successRate() const
{
    if (m_totalPropagations == 0) return 0.0;
    return (static_cast<double>(m_successfulPropagations) / m_totalPropagations) * 100.0;
}

void AspectPropagationService::recordProcessingTime(const QString& operation, double timeMs)
{
    if (!m_enablePerformanceMonitoring) return;
    
    m_processingTimes.append(timeMs);
    
    // Keep only recent measurements
    if (m_processingTimes.size() > PERFORMANCE_HISTORY_SIZE) {
        m_processingTimes.removeFirst();
    }
    
    updateAverageProcessingTime();
}

void AspectPropagationService::updateAverageProcessingTime()
{
    if (m_processingTimes.isEmpty()) {
        m_averageProcessingTimeMs = 0.0;
        return;
    }
    
    double sum = 0.0;
    for (double time : m_processingTimes) {
        sum += time;
    }
    
    m_averageProcessingTimeMs = sum / m_processingTimes.size();
    emit performanceChanged();
}

void AspectPropagationService::recordPropagationResult(const AspectPropagationResult& result)
{
    m_recentResults.append(result);
    
    // Keep only recent results
    if (m_recentResults.size() > RECENT_RESULTS_SIZE) {
        m_recentResults.removeFirst();
    }
    
    emit statisticsChanged();
}

QVariantMap AspectPropagationService::controlNodeToVariantMap(const ControlNode& node) const
{
    QVariantMap map;
    map["signalId"] = node.signalId;
    map["signalType"] = node.signalType;
    map["possibleAspects"] = node.possibleAspects;
    map["controlledBy"] = node.controlledBy;
    map["controls"] = node.controls;
    map["controlMode"] = node.controlMode;
    map["isIndependent"] = node.isIndependent;
    map["selectedAspect"] = node.selectedAspect;
    map["isProcessed"] = node.isProcessed;
    map["dependencyOrder"] = node.dependencyOrder;
    map["locationRow"] = node.locationRow;
    map["locationCol"] = node.locationCol;
    return map;
}

QVariantMap AspectPropagationService::controlEdgeToVariantMap(const ControlEdge& edge) const
{
    QVariantMap map;
    map["from"] = edge.fromSignalId;
    map["to"] = edge.toSignalId;
    map["whenAspect"] = edge.whenAspect;
    map["allowedAspects"] = edge.allowedAspects;
    map["conditions"] = edge.conditions;
    map["ruleId"] = edge.ruleId;
    return map;
}

QVariantMap AspectPropagationService::aspectPropagationResultToVariantMap(const AspectPropagationResult& result) const
{
    QVariantMap map;
    map["success"] = result.success;
    map["errorMessage"] = result.errorMessage;
    map["errorCode"] = result.errorCode;
    map["signalAspects"] = result.signalAspects;
    map["pointMachines"] = result.pointMachines;
    map["processedSignals"] = result.processedSignals;
    map["prunedSignals"] = result.prunedSignals;
    map["decisionReasons"] = result.decisionReasons;
    map["controlPath"] = result.controlPath;
    map["processingTimeMs"] = result.processingTimeMs;
    map["graphSize"] = result.graphSize;
    map["prunedGraphSize"] = result.prunedGraphSize;
    map["circularDependencies"] = result.circularDependencies;
    map["validationErrors"] = result.validationErrors;
    map["validationWarnings"] = result.validationWarnings;
    return map;
}

void AspectPropagationService::performPerformanceCheck()
{
    checkPerformanceThresholds();
}

void AspectPropagationService::checkPerformanceThresholds()
{
    if (m_averageProcessingTimeMs > WARNING_PROCESSING_TIME_MS) {
        emit performanceWarning("average_processing_time", 
                               m_averageProcessingTimeMs, 
                               WARNING_PROCESSING_TIME_MS);
    }
}

QVariantMap AspectPropagationService::validatePropagationRequestInternal(
    const QString& sourceSignalId,
    const QString& destinationSignalId)
{
    QVariantMap result;
    
    // Basic validation checks
    if (sourceSignalId.isEmpty() || destinationSignalId.isEmpty()) {
        result["success"] = false;
        result["error"] = "Signal IDs cannot be empty";
        result["errorCode"] = "EMPTY_SIGNAL_ID";
        return result;
    }
    
    if (sourceSignalId == destinationSignalId) {
        result["success"] = false;
        result["error"] = "Source and destination cannot be the same";
        result["errorCode"] = "SAME_SIGNAL";
        return result;
    }
    
    // Verify signals exist in database
    QVariantMap sourceSignal = m_dbManager->getSignalById(sourceSignalId);
    QVariantMap destSignal = m_dbManager->getSignalById(destinationSignalId);
    
    if (sourceSignal.isEmpty()) {
        result["success"] = false;
        result["error"] = "Source signal not found: " + sourceSignalId;
        result["errorCode"] = "SOURCE_NOT_FOUND";
        return result;
    }
    
    if (destSignal.isEmpty()) {
        result["success"] = false;
        result["error"] = "Destination signal not found: " + destinationSignalId;
        result["errorCode"] = "DEST_NOT_FOUND";
        return result;
    }
    
    result["success"] = true;
    result["message"] = "Propagation request validation passed";
    return result;
}

// Placeholder implementations for remaining methods - to be completed in subsequent iterations
QVariantMap AspectPropagationService::pruneGraphForDestination(
    const QVariantMap& fullGraph,
    const QString& destinationSignalId)
{
    return pruneGraphForDestinationInternal(fullGraph, destinationSignalId);
}

QVariantMap AspectPropagationService::pruneGraphForDestinationInternal(
    const QVariantMap& fullGraph,
    const QString& destinationSignalId)
{
    QElapsedTimer timer;
    timer.start();
    
    qDebug() << "✂️ [GRAPH_PRUNE] Pruning for destination:" << destinationSignalId;

    QVariantMap nodes = fullGraph["nodes"].toMap();
    QVariantList edges = fullGraph["edges"].toList();

    // If destination is not in the graph, return empty result
    if (!nodes.contains(destinationSignalId)) {
        qWarning() << "⚠️ [GRAPH_PRUNE] Destination signal not in control graph:" 
                   << destinationSignalId;
        
        QVariantMap emptyResult;
        emptyResult["success"] = false;
        emptyResult["error"] = "Destination signal not found in control graph";
        emptyResult["nodes"] = QVariantMap();
        emptyResult["edges"] = QVariantList();
        emptyResult["processingTimeMs"] = timer.elapsed();
        return emptyResult;
    }

    // Find the control path using breadth-first search from destination backwards
    QSet<QString> relevantSignals;
    QQueue<QString> toProcess;
    QSet<QString> visited;

    // Start with destination signal
    toProcess.enqueue(destinationSignalId);
    relevantSignals.insert(destinationSignalId);

    qDebug() << "🔍 [GRAPH_PRUNE] Finding control path to destination...";

    // Trace backwards through controlling signals
    while (!toProcess.isEmpty()) {
        QString currentSignal = toProcess.dequeue();
        if (visited.contains(currentSignal)) continue;
        visited.insert(currentSignal);

        QVariantMap nodeData = nodes[currentSignal].toMap();
        QStringList controlledBy = nodeData["controlledBy"].toStringList();

        qDebug() << "   📍" << currentSignal << "controlled by:" << controlledBy;

        for (const QString& controllingSignal : controlledBy) {
            if (nodes.contains(controllingSignal) && !relevantSignals.contains(controllingSignal)) {
                relevantSignals.insert(controllingSignal);
                toProcess.enqueue(controllingSignal);
                qDebug() << "     ➕ Added controlling signal:" << controllingSignal;
            }
        }
    }

    // Also include signals that control the relevant signals (upstream influencers)
    QSet<QString> additionalSignals;
    for (const QString& signalId : relevantSignals) {
        QVariantMap nodeData = nodes[signalId].toMap();
        QStringList controlledBy = nodeData["controlledBy"].toStringList();
        
        for (const QString& controllingSignal : controlledBy) {
            if (nodes.contains(controllingSignal) && !relevantSignals.contains(controllingSignal)) {
                additionalSignals.insert(controllingSignal);
                qDebug() << "     🔗 Added upstream influencer:" << controllingSignal;
            }
        }
    }
    
    relevantSignals.unite(additionalSignals);

    // Build pruned graph with only relevant signals
    QVariantMap prunedNodes;
    QVariantList prunedEdges;

    for (const QString& signalId : relevantSignals) {
        prunedNodes[signalId] = nodes[signalId];
    }

    for (const QVariant& edgeVariant : edges) {
        QVariantMap edge = edgeVariant.toMap();
        QString fromSignal = edge["from"].toString();
        QString toSignal = edge["to"].toString();

        if (relevantSignals.contains(fromSignal) && relevantSignals.contains(toSignal)) {
            prunedEdges.append(edge);
        }
    }

    qDebug() << "✂️ [GRAPH_PRUNE] Kept" << relevantSignals.size() << "relevant signals:"
             << relevantSignals.values();

    QVariantMap result;
    result["success"] = true;
    result["nodes"] = prunedNodes;
    result["edges"] = prunedEdges;
    result["processingTimeMs"] = timer.elapsed();
    result["originalSize"] = nodes.size();
    result["prunedSize"] = prunedNodes.size();
    result["relevantSignals"] = relevantSignals.values();
    
    recordProcessingTime("graph_pruning", timer.elapsed());
    
    return result;
}

QVector<ControlNode> AspectPropagationService::createDependencyOrder(const QVariantMap& prunedGraph)
{
    QElapsedTimer timer;
    timer.start();
    
    qDebug() << "📋 [DEPENDENCY_ORDER] Creating processing sequence...";

    QVariantMap nodes = prunedGraph["nodes"].toMap();
    QHash<QString, ControlNode> nodeHash;
    
    // Convert to ControlNode hash for easier processing
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        ControlNode node;
        QVariantMap nodeData = it.value().toMap();
        
        node.signalId = it.key();
        node.signalType = nodeData["signalType"].toString();
        node.possibleAspects = nodeData["possibleAspects"].toStringList();
        node.controlledBy = nodeData["controlledBy"].toStringList();
        node.controls = nodeData["controls"].toStringList();
        node.controlMode = nodeData["controlMode"].toString();
        node.isIndependent = nodeData["isIndependent"].toBool();
        node.locationRow = nodeData["locationRow"].toDouble();
        node.locationCol = nodeData["locationCol"].toDouble();
        
        nodeHash[it.key()] = node;
    }

    // Check for circular dependencies first
    QStringList circularSignals;
    if (m_enableCircularDependencyDetection && 
        detectCircularDependencies(nodeHash, circularSignals)) {
        qWarning() << "⚠️ [DEPENDENCY_ORDER] Circular dependencies detected:" << circularSignals;
        // Continue with algorithm, but note the issue
    }

    // Topological sort using Kahn's algorithm
    QVector<ControlNode> orderedNodes;
    QHash<QString, int> inDegree;
    QQueue<QString> independent;

    // Calculate in-degrees (number of controlling signals within the pruned graph)
    for (auto it = nodeHash.begin(); it != nodeHash.end(); ++it) {
        const QString& signalId = it.key();
        const ControlNode& node = it.value();
        // Only count controllers that are actually in the pruned graph
        QStringList relevantControllers;
        for (const QString& controller : node.controlledBy) {
            if (nodeHash.contains(controller)) {
                relevantControllers.append(controller);
            }
        }
        
        inDegree[signalId] = relevantControllers.size();
        
        if (node.isIndependent || relevantControllers.isEmpty()) {
            independent.enqueue(signalId);
            qDebug() << "   🆓 Independent signal:" << signalId 
                     << (node.isIndependent ? "[INDEPENDENT]" : "[NO_CONTROLLERS]");
        }
    }

    int order = 0;
    while (!independent.isEmpty()) {
        QString currentSignal = independent.dequeue();
        ControlNode node = nodeHash[currentSignal];
        node.dependencyOrder = order++;
        node.isProcessed = false; // Will be set during propagation
        
        orderedNodes.append(node);
        qDebug() << "   📝 Order" << node.dependencyOrder << ":" << currentSignal 
                 << "(" << node.signalType << ")";

        // Reduce in-degree for controlled signals that are in the pruned graph
        for (const QString& controlledSignal : node.controls) {
            if (nodeHash.contains(controlledSignal)) {
                inDegree[controlledSignal]--;
                if (inDegree[controlledSignal] == 0) {
                    independent.enqueue(controlledSignal);
                    qDebug() << "     ➡️ " << controlledSignal << "now ready for processing";
                }
            }
        }
    }

    // Check for circular dependencies by comparing processed vs total nodes
    if (orderedNodes.size() != nodeHash.size()) {
        qWarning() << "⚠️ [DEPENDENCY_ORDER] Circular dependency detected!"
                   << "Processed:" << orderedNodes.size() 
                   << "Total:" << nodeHash.size();
        
        // Add remaining nodes (this indicates a problem with the control rules)
        for (auto it = nodeHash.begin(); it != nodeHash.end(); ++it) {
            const QString& signalId = it.key();
            const ControlNode& node = it.value();
            bool found = false;
            for (const ControlNode& ordered : orderedNodes) {
                if (ordered.signalId == signalId) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                ControlNode problematicNode = node;
                problematicNode.dependencyOrder = order++;
                orderedNodes.append(problematicNode);
                qWarning() << "   ⚠️ Added problematic node:" << signalId;
            }
        }
    }

    qDebug() << "📋 [DEPENDENCY_ORDER] Complete processing sequence:" << orderedNodes.size() << "signals";
    
    recordProcessingTime("dependency_ordering", timer.elapsed());
    
    return orderedNodes;
}

bool AspectPropagationService::detectCircularDependencies(
    const QHash<QString, ControlNode>& nodes,
    QStringList& circularSignals)
{
    // Use depth-first search to detect cycles
    QHash<QString, int> state; // 0 = unvisited, 1 = visiting, 2 = visited
    bool hasCycle = false;
    
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        const QString& signalId = it.key();
        const ControlNode& node = it.value();
        if (state.value(signalId, 0) == 0) {
            if (detectCyclesDFS(signalId, nodes, state, circularSignals)) {
                hasCycle = true;
            }
        }
    }
    
    return hasCycle;
}

bool AspectPropagationService::detectCyclesDFS(
    const QString& signalId,
    const QHash<QString, ControlNode>& nodes,
    QHash<QString, int>& state,
    QStringList& circularSignals)
{
    state[signalId] = 1; // Mark as visiting
    
    if (nodes.contains(signalId)) {
        const ControlNode& node = nodes[signalId];
        
        for (const QString& controlledSignal : node.controls) {
            if (!nodes.contains(controlledSignal)) continue;
            
            int controlledState = state.value(controlledSignal, 0);
            
            if (controlledState == 1) {
                // Found a back edge - cycle detected
                circularSignals.append(signalId);
                circularSignals.append(controlledSignal);
                return true;
            } else if (controlledState == 0) {
                if (detectCyclesDFS(controlledSignal, nodes, state, circularSignals)) {
                    return true;
                }
            }
        }
    }
    
    state[signalId] = 2; // Mark as visited
    return false;
}

QVariantMap AspectPropagationService::selectOptimalAspects(
    const QVector<ControlNode>& orderedNodes,
    const QString& destinationSignalId,
    const QVariantMap& pointMachinePositions,
    const QVariantMap& options)
{
    QElapsedTimer timer;
    timer.start();
    
    qDebug() << "🎯 [ASPECT_SELECTION] Starting forward propagation...";

    QHash<QString, ControlNode> processedNodes;
    QVariantMap selectedAspects;
    QVariantMap requiredPointMachines;
    QVariantMap decisionReasons;
    QStringList processOrder;

    try {
        // Process nodes in dependency order
        for (ControlNode node : orderedNodes) {
            qDebug() << "🔄 [ASPECT_SELECTION] Processing:" << node.signalId 
                     << "(" << node.signalType << ")";

            processOrder.append(node.signalId);

            if (node.isIndependent) {
                // Independent signals can choose their aspect freely
                QString selectedAspect = selectBestAspect(
                    node, node.possibleAspects, destinationSignalId, 
                    node.signalId == destinationSignalId, options);
                    
                node.selectedAspect = selectedAspect;
                selectedAspects[node.signalId] = selectedAspect;
                
                decisionReasons[node.signalId] = QString(
                    "Independent signal - selected %1 (highest priority available)")
                    .arg(selectedAspect);
                    
                qDebug() << "   ✅ Independent choice:" << selectedAspect;
                
            } else {
                // Controlled signals must respect their controllers
                QStringList allowedByControllers = getAspectsAllowedByControllers(node, processedNodes);
                
                if (allowedByControllers.isEmpty()) {
                    QString errorMsg = QString("No valid aspects allowed by controlling signals for %1")
                                         .arg(node.signalId);
                    qCritical() << "❌ [ASPECT_SELECTION]" << errorMsg;
                    
                    QVariantMap errorResult;
                    errorResult["success"] = false;
                    errorResult["error"] = errorMsg;
                    errorResult["errorCode"] = "NO_VALID_ASPECTS";
                    errorResult["processingTimeMs"] = timer.elapsed();
                    return errorResult;
                }

                QString selectedAspect = selectBestAspect(
                    node, allowedByControllers, destinationSignalId,
                    node.signalId == destinationSignalId, options);
                    
                node.selectedAspect = selectedAspect;
                selectedAspects[node.signalId] = selectedAspect;
                
                decisionReasons[node.signalId] = QString(
                    "Controlled signal - selected %1 from allowed aspects: %2")
                    .arg(selectedAspect, allowedByControllers.join(","));
                    
                qDebug() << "   ✅ Controlled choice:" << selectedAspect 
                         << "from allowed:" << allowedByControllers;
            }

            // Validate the selection against all constraints
            if (!validateControlConstraints(node.signalId, node.selectedAspect, processedNodes)) {
                QString errorMsg = QString("Control constraint validation failed for %1 -> %2")
                                     .arg(node.signalId, node.selectedAspect);
                qCritical() << "❌ [ASPECT_SELECTION]" << errorMsg;
                
                QVariantMap errorResult;
                errorResult["success"] = false;
                errorResult["error"] = errorMsg;
                errorResult["errorCode"] = "CONSTRAINT_VIOLATION";
                errorResult["processingTimeMs"] = timer.elapsed();
                return errorResult;
            }

            node.isProcessed = true;
            processedNodes[node.signalId] = node;
        }

        qDebug() << "🎯 [ASPECT_SELECTION] Forward propagation completed successfully!";

        QVariantMap result;
        result["success"] = true;
        result["aspects"] = selectedAspects;
        result["pointMachines"] = requiredPointMachines;
        result["reasons"] = decisionReasons;
        result["processOrder"] = processOrder;
        result["processingTimeMs"] = timer.elapsed();
        
        recordProcessingTime("aspect_selection", timer.elapsed());
        
        return result;
        
    } catch (const std::exception& e) {
        qCritical() << "💥 [ASPECT_SELECTION] Exception:" << e.what();
        
        QVariantMap errorResult;
        errorResult["success"] = false;
        errorResult["error"] = QString("Aspect selection failed: %1").arg(e.what());
        errorResult["errorCode"] = "SELECTION_ERROR";
        errorResult["processingTimeMs"] = timer.elapsed();
        return errorResult;
    }
}

QString AspectPropagationService::selectBestAspect(
    const ControlNode& node,
    const QStringList& allowedAspects,
    const QString& destinationSignalId,
    bool isDestination,
    const QVariantMap& options)
{
    qDebug() << "🎯 [BEST_ASPECT] Selecting for" << node.signalId 
             << "from:" << allowedAspects 
             << "isDestination:" << isDestination;

    // Apply destination constraint if this is the destination signal
    if (isDestination) {
        QString constraint = m_destinationConstraints.value(node.signalType, "RED");
        
        if (constraint == "RED" && allowedAspects.contains("RED")) {
            qDebug() << "   🛑 Destination constraint: forcing RED for" << node.signalId;
            return "RED";
        } else if (constraint == "GREEN_OR_RED") {
            // Advanced Starter destinations may show GREEN if track clear
            if (allowedAspects.contains("GREEN")) {
                // TODO: Check track circuit occupancy in real implementation
                qDebug() << "   🟢 Destination allows GREEN for Advanced Starter";
                return "GREEN"; 
            } else if (allowedAspects.contains("RED")) {
                qDebug() << "   🛑 Destination fallback to RED";
                return "RED";
            }
        }
    }

    // Select highest priority aspect from allowed list
    QStringList priorities = getAspectPriorities(node.signalType);

    for (const QString& priorityAspect : priorities) {
        if (allowedAspects.contains(priorityAspect)) {
            qDebug() << "   🎯 Selected priority aspect:" << priorityAspect 
                     << "for" << node.signalType;
            return priorityAspect;
        }
    }

    // Fallback to first available aspect
    if (!allowedAspects.isEmpty()) {
        QString fallback = allowedAspects.first();
        qWarning() << "   ⚠️ Fallback aspect selection:" << fallback 
                   << "for" << node.signalId;
        return fallback;
    }

    qCritical() << "   💥 No aspects available for" << node.signalId;
    return "RED"; // Safety fallback
}

QStringList AspectPropagationService::getAspectsAllowedByControllers(
    const ControlNode& node,
    const QHash<QString, ControlNode>& processedNodes)
{
    if (node.controlledBy.isEmpty()) {
        return node.possibleAspects; // No controllers
    }

    QStringList allowedAspects;
    bool isFirstController = true;

    qDebug() << "   🔍 Checking controllers for" << node.signalId << ":" << node.controlledBy;

    for (const QString& controllingSignalId : node.controlledBy) {
        if (!processedNodes.contains(controllingSignalId)) {
            qWarning() << "   ⚠️ Controller not yet processed:" << controllingSignalId;
            continue; // Should not happen with proper dependency ordering
        }

        const ControlNode& controller = processedNodes[controllingSignalId];
        QStringList controllerAllowed = getAspectsPermittedByController(
            controller, node.signalId);

        qDebug() << "   📋 Controller" << controllingSignalId 
                 << "(" << controller.selectedAspect << ") allows:" << controllerAllowed;

        if (node.controlMode == "AND") {
            // All controllers must permit - intersection
            if (isFirstController) {
                allowedAspects = controllerAllowed;
                isFirstController = false;
            } else {
                QSet<QString> currentSet(allowedAspects.begin(), allowedAspects.end());
                QSet<QString> controllerSet(controllerAllowed.begin(), controllerAllowed.end());
                allowedAspects = (currentSet & controllerSet).values();
            }
        } else if (node.controlMode == "OR") {
            // Any controller can permit - union
            QSet<QString> currentSet(allowedAspects.begin(), allowedAspects.end());
            QSet<QString> controllerSet(controllerAllowed.begin(), controllerAllowed.end());
            allowedAspects = (currentSet | controllerSet).values();
        }
    }

    // Filter to only aspects this signal can actually display
    QSet<QString> possibleSet(node.possibleAspects.begin(), node.possibleAspects.end());
    QSet<QString> allowedSet(allowedAspects.begin(), allowedAspects.end());
    QStringList finalAllowed = (possibleSet & allowedSet).values();

    qDebug() << "   ✅ Final allowed aspects for" << node.signalId << ":" << finalAllowed;
    return finalAllowed;
}

QStringList AspectPropagationService::getAspectsPermittedByController(
    const ControlNode& controller,
    const QString& controlledSignalId)
{
    // This integrates with the control rules - simplified logic for now
    QString controllerAspect = controller.selectedAspect;
    
    qDebug() << "   🔗 Controller" << controller.signalId 
             << "aspect" << controllerAspect
             << "controlling" << controlledSignalId;
    
    if (controllerAspect == "GREEN") {
        return QStringList{"GREEN", "YELLOW", "RED"}; // All aspects permitted
    } else if (controllerAspect == "YELLOW") {
        return QStringList{"YELLOW", "RED"}; // Restricted aspects
    } else if (controllerAspect == "RED") {
        return QStringList{"RED"}; // Only danger aspect
    }
    
    return QStringList{"RED"}; // Safe default
}

bool AspectPropagationService::validateControlConstraints(
    const QString& signalId,
    const QString& selectedAspect,
    const QHash<QString, ControlNode>& processedNodes)
{
    // Validate that the selected aspect is actually permitted by all controlling signals
    // This is a final safety check before committing the aspect selection
    
    qDebug() << "   🔒 Validating constraints for" << signalId << "→" << selectedAspect;
    
    if (m_ruleEngine) {
        // In a full implementation, this would use the interlocking rule engine
        // For now, perform basic validation
        
        // Check if the aspect is in the signal's possible aspects
        QVariantMap signalData = m_dbManager->getSignalById(signalId);
        QStringList possibleAspects = signalData["possibleAspects"].toStringList();
        
        if (!possibleAspects.contains(selectedAspect)) {
            qWarning() << "   ⚠️ Aspect" << selectedAspect << "not possible for signal" << signalId;
            return false;
        }
    }
    
    qDebug() << "   ✅ Constraints validated for" << signalId;
    return true;
}

QStringList AspectPropagationService::getAspectPriorities(const QString& signalType) const
{
    return m_aspectPriorities.value(signalType, QStringList{"GREEN", "YELLOW", "RED"});
}

// Additional slot implementations
void AspectPropagationService::onSignalAspectChanged(const QString& signalId, const QString& newAspect)
{
    // Clear cache for the changed signal
    m_signalDataCache.remove(signalId);
    
    qDebug() << "🔄 [ASPECT_PROPAGATION] Signal aspect changed:" << signalId << "→" << newAspect;
}

void AspectPropagationService::onInterlockingRulesChanged()
{
    // Clear all caches when interlocking rules change
    m_signalDataCache.clear();
    m_controlRelationshipCache.clear();
    
    qDebug() << "🔄 [ASPECT_PROPAGATION] Interlocking rules changed - cache cleared";
}